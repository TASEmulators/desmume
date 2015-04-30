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
OGLEXT(PFNGLCLEARBUFFERFVPROC, glClearBufferfv) // Core in v3.0
OGLEXT(PFNGLCLEARBUFFERFIPROC, glClearBufferfi) // Core in v3.0

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
	INITOGLEXT(PFNGLCLEARBUFFERFVPROC, glClearBufferfv)
	INITOGLEXT(PFNGLCLEARBUFFERFIPROC, glClearBufferfi)
	
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

// Vertex shader for geometry, GLSL 1.50
static const char *GeometryVtxShader_150 = {"\
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

// Fragment shader for geometry, GLSL 1.50
static const char *GeometryFragShader_150 = {"\
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
	uniform bool stateUseWDepth; \n\
	uniform float stateAlphaTestRef; \n\
	\n\
	uniform int polyMode; \n\
	uniform bool polyEnableDepthWrite;\n\
	uniform bool polySetNewDepthForTranslucent;\n\
	uniform int polyID; \n\
	\n\
	uniform bool polyEnableTexture; \n\
	uniform bool polyEnableFog;\n\
	\n\
	out vec4 outFragColor;\n\
	out vec4 outFragDepth;\n\
	out vec4 outPolyID;\n\
	out vec4 outFogAttributes;\n\
	\n\
	vec3 packVec3FromFloat(const float value)\n\
	{\n\
		float expandedValue = value * 16777215.0;\n\
		vec3 packedValue = vec3( fract(expandedValue/256.0), fract(((expandedValue/256.0) - fract(expandedValue/256.0)) / 256.0), fract(((expandedValue/65536.0) - fract(expandedValue/65536.0)) / 256.0) );\n\
		return packedValue;\n\
	}\n\
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
		outFragDepth = vec4( packVec3FromFloat(newFragDepth), float(polyEnableDepthWrite && (newFragColor.a > 0.999 || polySetNewDepthForTranslucent)));\n\
		outPolyID = vec4(float(polyID)/63.0, 0.0, 0.0, float(newFragColor.a > 0.999));\n\
		outFogAttributes = vec4( float(polyEnableFog), 0.0, 0.0, float(newFragColor.a > 0.999 || !polyEnableFog));\n\
		gl_FragDepth = newFragDepth;\n\
	} \n\
"};

// Vertex shader for applying edge marking, GLSL 1.50
static const char *EdgeMarkVtxShader_150 = {"\
	#version 150\n\
	\n\
	in vec2 inPosition;\n\
	in vec2 inTexCoord0;\n\
	uniform vec2 framebufferSize;\n\
	out vec2 texCoord[5];\n\
	\n\
	void main()\n\
	{\n\
		vec2 texInvScale = vec2(1.0/framebufferSize.x, 1.0/framebufferSize.y);\n\
		\n\
		texCoord[0] = inTexCoord0; // Center\n\
		texCoord[1] = inTexCoord0 + (vec2( 1.0, 0.0) * texInvScale); // Right\n\
		texCoord[2] = inTexCoord0 + (vec2( 0.0, 1.0) * texInvScale); // Down\n\
		texCoord[3] = inTexCoord0 + (vec2(-1.0, 0.0) * texInvScale); // Left\n\
		texCoord[4] = inTexCoord0 + (vec2( 0.0,-1.0) * texInvScale); // Up\n\
		\n\
		gl_Position = vec4(inPosition, 0.0, 1.0);\n\
	}\n\
"};

// Fragment shader for applying edge marking, GLSL 1.50
static const char *EdgeMarkFragShader_150 = {"\
	#version 150\n\
	\n\
	in vec2 texCoord[5];\n\
	\n\
	uniform sampler2D texInFragDepth;\n\
	uniform sampler2D texInPolyID;\n\
	uniform vec4 stateEdgeColor[8];\n\
	\n\
	out vec4 outFragColor;\n\
	\n\
	float unpackFloatFromVec3(const vec3 value)\n\
	{\n\
		const vec3 unpackRatio = vec3(256.0, 65536.0, 16777216.0);\n\
		return (dot(value, unpackRatio) / 16777215.0);\n\
	}\n\
	\n\
	void main()\n\
	{\n\
		vec4 inPolyIDAttributes[5];\n\
		inPolyIDAttributes[0] = texture(texInPolyID, texCoord[0]);\n\
		inPolyIDAttributes[1] = texture(texInPolyID, texCoord[1]);\n\
		inPolyIDAttributes[2] = texture(texInPolyID, texCoord[2]);\n\
		inPolyIDAttributes[3] = texture(texInPolyID, texCoord[3]);\n\
		inPolyIDAttributes[4] = texture(texInPolyID, texCoord[4]);\n\
		\n\
		int polyID[5];\n\
		polyID[0] = int((inPolyIDAttributes[0].r * 63.0) + 0.5);\n\
		polyID[1] = int((inPolyIDAttributes[1].r * 63.0) + 0.5);\n\
		polyID[2] = int((inPolyIDAttributes[2].r * 63.0) + 0.5);\n\
		polyID[3] = int((inPolyIDAttributes[3].r * 63.0) + 0.5);\n\
		polyID[4] = int((inPolyIDAttributes[4].r * 63.0) + 0.5);\n\
		\n\
		float depth[5];\n\
		depth[0] = unpackFloatFromVec3(texture(texInFragDepth, texCoord[0]).rgb);\n\
		depth[1] = unpackFloatFromVec3(texture(texInFragDepth, texCoord[1]).rgb);\n\
		depth[2] = unpackFloatFromVec3(texture(texInFragDepth, texCoord[2]).rgb);\n\
		depth[3] = unpackFloatFromVec3(texture(texInFragDepth, texCoord[3]).rgb);\n\
		depth[4] = unpackFloatFromVec3(texture(texInFragDepth, texCoord[4]).rgb);\n\
		\n\
		vec4 edgeColor = vec4(0.0, 0.0, 0.0, 0.0);\n\
		\n\
		for (int i = 1; i < 5; i++)\n\
		{\n\
			if (polyID[0] != polyID[i] && depth[0] >= depth[i])\n\
			{\n\
				edgeColor = stateEdgeColor[polyID[i]/8];\n\
				break;\n\
			}\n\
		}\n\
		\n\
		outFragColor = edgeColor;\n\
	}\n\
"};

// Vertex shader for applying fog, GLSL 1.50
static const char *FogVtxShader_150 = {"\
	#version 150\n\
	\n\
	in vec2 inPosition;\n\
	in vec2 inTexCoord0;\n\
	out vec2 texCoord;\n\
	\n\
	void main()\n\
	{\n\
		texCoord = inTexCoord0;\n\
		gl_Position = vec4(inPosition, 0.0, 1.0);\n\
	}\n\
"};

// Fragment shader for applying fog, GLSL 1.50
static const char *FogFragShader_150 = {"\
	#version 150\n\
	\n\
	in vec2 texCoord;\n\
	\n\
	uniform sampler2D texInFragColor;\n\
	uniform sampler2D texInFragDepth;\n\
	uniform sampler2D texInFogAttributes;\n\
	uniform bool stateEnableFogAlphaOnly;\n\
	uniform vec4 stateFogColor;\n\
	uniform float stateFogDensity[32];\n\
	uniform float stateFogOffset;\n\
	uniform float stateFogStep;\n\
	\n\
	out vec4 outFragColor;\n\
	\n\
	float unpackFloatFromVec3(const vec3 value)\n\
	{\n\
		const vec3 unpackRatio = vec3(256.0, 65536.0, 16777216.0);\n\
		return (dot(value, unpackRatio) / 16777215.0);\n\
	}\n\
	\n\
	void main()\n\
	{\n\
		vec4 inFragColor = texture(texInFragColor, texCoord);\n\
		vec4 inFogAttributes = texture(texInFogAttributes, texCoord);\n\
		bool polyEnableFog = bool(inFogAttributes.r);\n\
		vec4 newFoggedColor = inFragColor;\n\
		\n\
		if (polyEnableFog)\n\
		{\n\
			float inFragDepth = unpackFloatFromVec3(texture(texInFragDepth, texCoord).rgb);\n\
			float fogMixWeight = 0.0;\n\
			\n\
			if (inFragDepth <= min(stateFogOffset + stateFogStep, 1.0))\n\
			{\n\
				fogMixWeight = stateFogDensity[0];\n\
			}\n\
			else if (inFragDepth >= min(stateFogOffset + (stateFogStep*32.0), 1.0))\n\
			{\n\
				fogMixWeight = stateFogDensity[31];\n\
			}\n\
			else\n\
			{\n\
				for (int i = 1; i < 32; i++)\n\
				{\n\
					float currentFogStep = min(stateFogOffset + (stateFogStep * float(i+1)), 1.0);\n\
					if (inFragDepth <= currentFogStep)\n\
					{\n\
						float previousFogStep = min(stateFogOffset + (stateFogStep * float(i)), 1.0);\n\
						fogMixWeight = mix(stateFogDensity[i-1], stateFogDensity[i], (inFragDepth - previousFogStep) / (currentFogStep - previousFogStep));\n\
						break;\n\
					}\n\
				}\n\
			}\n\
			\n\
			newFoggedColor = mix(inFragColor, (stateEnableFogAlphaOnly) ? vec4(inFragColor.rgb, stateFogColor.a) : stateFogColor, fogMixWeight);\n\
		}\n\
		\n\
		outFragColor = newFoggedColor;\n\
	}\n\
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
	error = this->LoadGeometryShaders(vertexShaderProgram, fragmentShaderProgram);
	if (error != OGLERROR_NOERR)
	{
		this->isShaderSupported = false;
		return error;
	}
	
	error = this->InitGeometryProgram(vertexShaderProgram, fragmentShaderProgram);
	if (error != OGLERROR_NOERR)
	{
		this->isShaderSupported = false;
		return error;
	}
	
	std::string edgeMarkVtxShaderString = std::string(EdgeMarkVtxShader_150);
	std::string edgeMarkFragShaderString = std::string(EdgeMarkFragShader_150);
	std::string fogVtxShaderString = std::string(FogVtxShader_150);
	std::string fogFragShaderString = std::string(FogFragShader_150);
	error = this->InitPostprocessingPrograms(edgeMarkVtxShaderString, edgeMarkFragShaderString, fogVtxShaderString, fogFragShaderString);
	if (error != OGLERROR_NOERR)
	{
		this->DestroyGeometryProgram();
		this->isShaderSupported = false;
		return error;
	}
	
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

Render3DError OpenGLRenderer_3_2::InitEdgeMarkProgramBindings()
{
	OGLRenderRef &OGLRef = *this->ref;
	glBindAttribLocation(OGLRef.programEdgeMarkID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programEdgeMarkID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	glBindFragDataLocation(OGLRef.programEdgeMarkID, 0, "outFragColor");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::InitFogProgramBindings()
{
	OGLRenderRef &OGLRef = *this->ref;
	glBindAttribLocation(OGLRef.programFogID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programFogID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	glBindFragDataLocation(OGLRef.programFogID, 0, "outFragColor");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::CreateFBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up FBO render targets
	glGenTextures(1, &OGLRef.texGColorID);
	glGenTextures(1, &OGLRef.texGDepthID);
	glGenTextures(1, &OGLRef.texGPolyID);
	glGenTextures(1, &OGLRef.texGFogAttrID);
	glGenTextures(1, &OGLRef.texGDepthStencilID);
	glGenTextures(1, &OGLRef.texPostprocessFogID);
	
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
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGPolyID);
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
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texPostprocessFogID);
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
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, OGLRef.texGPolyID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, OGLRef.texGFogAttrID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, OGLRef.texGDepthStencilID, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to created FBOs. Some emulation features will be disabled.\n");
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &OGLRef.fboRenderID);
		glDeleteFramebuffers(1, &OGLRef.fboPostprocessID);
		glDeleteTextures(1, &OGLRef.texGColorID);
		glDeleteTextures(1, &OGLRef.texGDepthID);
		glDeleteTextures(1, &OGLRef.texGPolyID);
		glDeleteTextures(1, &OGLRef.texGFogAttrID);
		glDeleteTextures(1, &OGLRef.texGDepthStencilID);
		glDeleteTextures(1, &OGLRef.texPostprocessFogID);
		
		OGLRef.fboRenderID = 0;
		OGLRef.fboPostprocessID = 0;
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glDrawBuffers(4, RenderDrawList);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboPostprocessID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, OGLRef.texPostprocessFogID, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to created FBOs. Some emulation features will be disabled.\n");
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &OGLRef.fboRenderID);
		glDeleteFramebuffers(1, &OGLRef.fboPostprocessID);
		glDeleteTextures(1, &OGLRef.texGColorID);
		glDeleteTextures(1, &OGLRef.texGDepthID);
		glDeleteTextures(1, &OGLRef.texGPolyID);
		glDeleteTextures(1, &OGLRef.texGFogAttrID);
		glDeleteTextures(1, &OGLRef.texGDepthStencilID);
		glDeleteTextures(1, &OGLRef.texPostprocessFogID);
		
		OGLRef.fboRenderID = 0;
		OGLRef.fboPostprocessID = 0;
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	
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
	glDeleteTextures(1, &OGLRef.texGDepthID);
	glDeleteTextures(1, &OGLRef.texGPolyID);
	glDeleteTextures(1, &OGLRef.texGFogAttrID);
	glDeleteTextures(1, &OGLRef.texGDepthStencilID);
	glDeleteTextures(1, &OGLRef.texPostprocessFogID);
	
	OGLRef.fboRenderID = 0;
	OGLRef.fboPostprocessID = 0;
	
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
	glGenRenderbuffers(1, &OGLRef.rboMSGPolyID);
	glGenRenderbuffers(1, &OGLRef.rboMSGFogAttrID);
	glGenRenderbuffers(1, &OGLRef.rboMSGDepthStencilID);
	
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGColorID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGDepthID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGPolyID);
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
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_RENDERBUFFER, OGLRef.rboMSGPolyID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_RENDERBUFFER, OGLRef.rboMSGFogAttrID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, OGLRef.rboMSGDepthStencilID);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to create multisampled FBO. Multisample antialiasing will be disabled.\n");
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &OGLRef.fboMSIntermediateRenderID);
		glDeleteRenderbuffers(1, &OGLRef.rboMSGColorID);
		glDeleteRenderbuffers(1, &OGLRef.rboMSGDepthID);
		glDeleteRenderbuffers(1, &OGLRef.rboMSGPolyID);
		glDeleteRenderbuffers(1, &OGLRef.rboMSGFogAttrID);
		glDeleteRenderbuffers(1, &OGLRef.rboMSGDepthStencilID);
		
		OGLRef.fboMSIntermediateRenderID = 0;
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboRenderID);
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
	glDeleteRenderbuffers(1, &OGLRef.rboMSGPolyID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGFogAttrID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGDepthStencilID);
	
	OGLRef.fboMSIntermediateRenderID = 0;
	
	this->isMultisampledFBOSupported = false;
}

Render3DError OpenGLRenderer_3_2::CreateVAOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenVertexArrays(1, &OGLRef.vaoGeometryStatesID);
	glGenVertexArrays(1, &OGLRef.vaoPostprocessStatesID);
	
	glBindVertexArray(OGLRef.vaoGeometryStatesID);
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
	
	glEnableVertexAttribArray(OGLVertexAttributeID_Position);
	glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	glEnableVertexAttribArray(OGLVertexAttributeID_Color);
	glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
	glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
	glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
	
	glBindVertexArray(0);
	
	glBindVertexArray(OGLRef.vaoPostprocessStatesID);
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboPostprocessIndexID);
	
	glEnableVertexAttribArray(OGLVertexAttributeID_Position);
	glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(sizeof(GLfloat) * 8));
	
	glBindVertexArray(0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_3_2::DestroyVAOs()
{
	if (!this->isVAOSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &OGLRef.vaoGeometryStatesID);
	glDeleteVertexArrays(1, &OGLRef.vaoPostprocessStatesID);
	
	this->isVAOSupported = false;
}

Render3DError OpenGLRenderer_3_2::LoadGeometryShaders(std::string &outVertexShaderProgram, std::string &outFragmentShaderProgram)
{
	outVertexShaderProgram.clear();
	outFragmentShaderProgram.clear();
	
	outVertexShaderProgram = std::string(GeometryVtxShader_150);
	outFragmentShaderProgram = std::string(GeometryFragShader_150);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::InitGeometryProgramBindings()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindAttribLocation(OGLRef.programGeometryID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programGeometryID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	glBindAttribLocation(OGLRef.programGeometryID, OGLVertexAttributeID_Color, "inColor");
	glBindFragDataLocation(OGLRef.programGeometryID, 0, "outFragColor");
	glBindFragDataLocation(OGLRef.programGeometryID, 1, "outFragDepth");
	glBindFragDataLocation(OGLRef.programGeometryID, 2, "outPolyID");
	glBindFragDataLocation(OGLRef.programGeometryID, 3, "outFogAttributes");
	
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
	
	glBindVertexArray(OGLRef.vaoGeometryStatesID);
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
	
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
	glDrawBuffers(4, RenderDrawList);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::DownsampleFBO()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (OGLRef.selectedRenderingFBO == OGLRef.fboMSIntermediateRenderID)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.fboMSIntermediateRenderID);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.fboRenderID);
		
		// Blit the color buffer
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the working depth buffer
		glReadBuffer(GL_COLOR_ATTACHMENT1);
		glDrawBuffer(GL_COLOR_ATTACHMENT1);
		glBlitFramebuffer(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the polygon ID buffer
		glReadBuffer(GL_COLOR_ATTACHMENT2);
		glDrawBuffer(GL_COLOR_ATTACHMENT2);
		glBlitFramebuffer(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the fog buffer
		glReadBuffer(GL_COLOR_ATTACHMENT3);
		glDrawBuffer(GL_COLOR_ATTACHMENT3);
		glBlitFramebuffer(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Reset framebuffer targets
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffers(4, RenderDrawList);
		glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboRenderID);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::RenderEdgeMarking(const u16 *colorTable, const bool useAntialias)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	const GLfloat alpha = (useAntialias) ? (16.0f/31.0f) : 1.0f;
	const GLfloat oglColor[4*8]	= {divide5bitBy31_LUT[(colorTable[0]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[0] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[0] >> 10) & 0x001F],
								   alpha,
								   divide5bitBy31_LUT[(colorTable[1]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[1] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[1] >> 10) & 0x001F],
								   alpha,
								   divide5bitBy31_LUT[(colorTable[2]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[2] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[2] >> 10) & 0x001F],
								   alpha,
								   divide5bitBy31_LUT[(colorTable[3]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[3] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[3] >> 10) & 0x001F],
								   alpha,
								   divide5bitBy31_LUT[(colorTable[4]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[4] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[4] >> 10) & 0x001F],
								   alpha,
								   divide5bitBy31_LUT[(colorTable[5]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[5] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[5] >> 10) & 0x001F],
								   alpha,
								   divide5bitBy31_LUT[(colorTable[6]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[6] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[6] >> 10) & 0x001F],
								   alpha,
								   divide5bitBy31_LUT[(colorTable[7]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[7] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[7] >> 10) & 0x001F],
								   alpha};
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboRenderID);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glUseProgram(OGLRef.programEdgeMarkID);
	glUniform1i(OGLRef.uniformTexGDepth_EdgeMark, OGLTextureUnitID_GDepth);
	glUniform1i(OGLRef.uniformTexGPolyID_EdgeMark, OGLTextureUnitID_GPolyID);
	glUniform2f(OGLRef.uniformFramebufferSize, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glUniform4fv(OGLRef.uniformStateEdgeColor, 8, oglColor);
	
	glViewport(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboPostprocessIndexID);
	glBindVertexArray(OGLRef.vaoPostprocessStatesID);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GDepth);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthID);
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GPolyID);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGPolyID);
	
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
	
	glBindVertexArray(0);
	
	return RENDER3DERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::RenderFog(const u8 *densityTable, const u32 color, const u32 offset, const u8 shift, const bool alphaOnly)
{
	OGLRenderRef &OGLRef = *this->ref;
	static GLfloat oglDensityTable[32];
	
	for (size_t i = 0; i < 32; i++)
	{
		oglDensityTable[i] = (densityTable[i] == 127) ? 1.0f : (GLfloat)densityTable[i] / 128.0f;
	}
	
	const GLfloat oglColor[4]	= {divide5bitBy31_LUT[(color      ) & 0x0000001F],
								   divide5bitBy31_LUT[(color >>  5) & 0x0000001F],
								   divide5bitBy31_LUT[(color >> 10) & 0x0000001F],
								   divide5bitBy31_LUT[(color >> 16) & 0x0000001F]};
	
	const GLfloat oglOffset = (GLfloat)offset / 32767.0f;
	const GLfloat oglFogStep = (GLfloat)(0x0400 >> shift) / 32767.0f;
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboPostprocessID);
	glUseProgram(OGLRef.programFogID);
	glUniform1i(OGLRef.uniformTexGColor_Fog, OGLTextureUnitID_GColor);
	glUniform1i(OGLRef.uniformTexGDepth_Fog, OGLTextureUnitID_GDepth);
	glUniform1i(OGLRef.uniformTexGFog_Fog, OGLTextureUnitID_FogAttr);
	glUniform1i(OGLRef.uniformStateEnableFogAlphaOnly, (alphaOnly) ? GL_TRUE : GL_FALSE);
	glUniform4f(OGLRef.uniformStateFogColor, oglColor[0], oglColor[1], oglColor[2], oglColor[3]);
	glUniform1f(OGLRef.uniformStateFogOffset, oglOffset);
	glUniform1f(OGLRef.uniformStateFogStep, oglFogStep);
	glUniform1fv(OGLRef.uniformStateFogDensity, 32, oglDensityTable);
	
	glViewport(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboPostprocessIndexID);
	glBindVertexArray(OGLRef.vaoPostprocessStatesID);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GColor);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGColorID);
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GDepth);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthID);
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FogAttr);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGFogAttrID);
	
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
	
	glBindVertexArray(0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const bool *__restrict fogBuffer, const u8 *__restrict polyIDBuffer)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	this->UploadClearImage(colorBuffer, depthBuffer, fogBuffer, polyIDBuffer);
	
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
		
		// Blit the polygon ID buffer
		glReadBuffer(GL_COLOR_ATTACHMENT2);
		glDrawBuffer(GL_COLOR_ATTACHMENT2);
		glBlitFramebuffer(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the fog buffer
		glReadBuffer(GL_COLOR_ATTACHMENT3);
		glDrawBuffer(GL_COLOR_ATTACHMENT3);
		glBlitFramebuffer(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Reset framebuffer targets
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffers(4, RenderDrawList);
		glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboMSIntermediateRenderID);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::ClearUsingValues(const FragmentColor &clearColor, const FragmentAttributes &clearAttributes) const
{
	const GLfloat oglColor[4] = {divide5bitBy31_LUT[clearColor.r], divide5bitBy31_LUT[clearColor.g], divide5bitBy31_LUT[clearColor.b], divide5bitBy31_LUT[clearColor.a]};
	const GLfloat oglDepth[4] = {(GLfloat)(clearAttributes.depth & 0x000000FF)/255.0f, (GLfloat)((clearAttributes.depth >> 8) & 0x000000FF)/255.0f, (GLfloat)((clearAttributes.depth >> 16) & 0x000000FF)/255.0f, 1.0};
	const GLfloat oglPolyID[4] = {(GLfloat)clearAttributes.opaquePolyID/63.0f, 0.0, 0.0, 1.0};
	const GLfloat oglFogAttr[4] = {(clearAttributes.isFogged) ? 1.0 : 0.0, 0.0, 0.0, 1.0};
	
	glClearBufferfi(GL_DEPTH_STENCIL, 0, (GLfloat)clearAttributes.depth / (GLfloat)0x00FFFFFF, 0);
	glClearBufferfv(GL_COLOR, 0, oglColor); // texGColorID
	glClearBufferfv(GL_COLOR, 1, oglDepth); // texGDepthID
	glClearBufferfv(GL_COLOR, 2, oglPolyID); // texGPolyID
	glClearBufferfv(GL_COLOR, 3, oglFogAttr); // texGFogAttrID
	
	return OGLERROR_NOERR;
}
