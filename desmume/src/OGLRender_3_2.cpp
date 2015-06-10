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

// Buffer Objects
OGLEXT(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange) // Core in v3.0

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

// UBO
OGLEXT(PFNGLGETUNIFORMBLOCKINDEXPROC, glGetUniformBlockIndex) // Core in v3.1
OGLEXT(PFNGLUNIFORMBLOCKBINDINGPROC, glUniformBlockBinding) // Core in v3.1
OGLEXT(PFNGLBINDBUFFERBASEPROC, glBindBufferBase) // Core in v3.0
OGLEXT(PFNGLGETACTIVEUNIFORMBLOCKIVPROC, glGetActiveUniformBlockiv) // Core in v3.1

// TBO
OGLEXT(PFNGLTEXBUFFERPROC, glTexBuffer) // Core in v3.1

void OGLLoadEntryPoints_3_2()
{
	// Basic Functions
	INITOGLEXT(PFNGLGETSTRINGIPROC, glGetStringi)
	INITOGLEXT(PFNGLCLEARBUFFERFVPROC, glClearBufferfv)
	INITOGLEXT(PFNGLCLEARBUFFERFIPROC, glClearBufferfi)
	
	// Shaders
	INITOGLEXT(PFNGLBINDFRAGDATALOCATIONPROC, glBindFragDataLocation)
	
	// Buffer Objects
	INITOGLEXT(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange)
	
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
	
	// UBO
	INITOGLEXT(PFNGLGETUNIFORMBLOCKINDEXPROC, glGetUniformBlockIndex)
	INITOGLEXT(PFNGLUNIFORMBLOCKBINDINGPROC, glUniformBlockBinding)
	INITOGLEXT(PFNGLBINDBUFFERBASEPROC, glBindBufferBase)
	INITOGLEXT(PFNGLGETACTIVEUNIFORMBLOCKIVPROC, glGetActiveUniformBlockiv)
	
	// TBO
	INITOGLEXT(PFNGLTEXBUFFERPROC, glTexBuffer)
}

// Vertex shader for geometry, GLSL 1.50
static const char *GeometryVtxShader_150 = {"\
	#version 150 \n\
	\n\
	in vec4 inPosition; \n\
	in vec2 inTexCoord0; \n\
	in vec3 inColor; \n\
	\n\
	uniform usamplerBuffer PolyStates;\n\
	uniform int polyIndex;\n\
	\n\
	out vec4 vtxPosition; \n\
	out vec2 vtxTexCoord; \n\
	out vec4 vtxColor; \n\
	flat out uint polyEnableTexture;\n\
	flat out uint polyEnableFog;\n\
	flat out uint polyEnableDepthWrite;\n\
	flat out uint polySetNewDepthForTranslucent;\n\
	flat out uint polyMode;\n\
	flat out uint polyID;\n\
	\n\
	void main() \n\
	{ \n\
		uvec4 polyStateFlags = texelFetch(PolyStates, (polyIndex*3)+0);\n\
		uvec4 polyStateValues = texelFetch(PolyStates, (polyIndex*3)+1);\n\
		uvec4 polyStateTexParams = texelFetch(PolyStates, (polyIndex*3)+2);\n\
		\n\
		float polyAlpha = float(polyStateValues[0]) / 31.0;\n\
		vec2 polyTexScale = vec2(1.0 / float(8 << polyStateTexParams[0]), 1.0 / float(8 << polyStateTexParams[1]));\n\
		\n\
		polyEnableTexture = polyStateFlags[0];\n\
		polyEnableFog = polyStateFlags[1];\n\
		polyEnableDepthWrite = polyStateFlags[2];\n\
		polySetNewDepthForTranslucent = polyStateFlags[3];\n\
		polyMode = polyStateValues[1];\n\
		polyID = polyStateValues[2];\n\
		\n\
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
	flat in uint polyEnableTexture;\n\
	flat in uint polyEnableFog;\n\
	flat in uint polyEnableDepthWrite;\n\
	flat in uint polySetNewDepthForTranslucent;\n\
	flat in uint polyMode;\n\
	flat in uint polyID;\n\
	\n\
	layout (std140) uniform RenderStates\n\
	{\n\
		vec2 framebufferSize;\n\
		int toonShadingMode;\n\
		bool enableAlphaTest;\n\
		bool enableAntialiasing;\n\
		bool enableEdgeMarking;\n\
		bool enableFogAlphaOnly;\n\
		bool useWDepth;\n\
		float alphaTestRef;\n\
		float fogOffset;\n\
		float fogStep;\n\
		float pad_0;\n\
		vec4 fogColor;\n\
		float fogDensity[32];\n\
		vec4 edgeColor[8];\n\
		vec4 toonColor[32];\n\
	} state;\n\
	\n\
	uniform sampler2D texRenderObject; \n\
	uniform usamplerBuffer PolyStates;\n\
	uniform int polyIndex;\n\
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
		vec4 mainTexColor = bool(polyEnableTexture) ? texture(texRenderObject, vtxTexCoord) : vec4(1.0, 1.0, 1.0, 1.0);\n\
		vec4 newFragColor = mainTexColor * vtxColor; \n\
		\n\
		if (polyMode == 1u) \n\
		{ \n\
			newFragColor.rgb = bool(polyEnableTexture) ? mix(vtxColor.rgb, mainTexColor.rgb, mainTexColor.a) : vtxColor.rgb;\n\
			newFragColor.a = vtxColor.a; \n\
		} \n\
		else if (polyMode == 2u) \n\
		{ \n\
			vec3 newToonColor = state.toonColor[int((vtxColor.r * 31.0) + 0.5)].rgb;\n\
			newFragColor.rgb = (state.toonShadingMode == 0) ? mainTexColor.rgb * newToonColor.rgb : min((mainTexColor.rgb * vtxColor.rgb) + newToonColor.rgb, 1.0); \n\
		} \n\
		else if (polyMode == 3u) \n\
		{ \n\
			if (polyID != 0u) \n\
			{ \n\
				newFragColor = vtxColor; \n\
			} \n\
		} \n\
		\n\
		if (newFragColor.a == 0.0 || (state.enableAlphaTest && newFragColor.a < state.alphaTestRef)) \n\
		{ \n\
			discard; \n\
		} \n\
		\n\
		float vertW = (vtxPosition.w == 0.0) ? 0.00000001 : vtxPosition.w; \n\
		// hack: when using z-depth, drop some LSBs so that the overworld map in Dragon Quest IV shows up correctly\n\
		float newFragDepth = (state.useWDepth) ? vtxPosition.w/4096.0 : clamp( (floor((((vtxPosition.z/vertW) * 0.5 + 0.5) * 16777215.0) / 4.0) * 4.0) / 16777215.0, 0.0, 1.0); \n\
		\n\
		outFragColor = newFragColor;\n\
		outFragDepth = vec4( packVec3FromFloat(newFragDepth), float(bool(polyEnableDepthWrite) && (newFragColor.a > 0.999 || bool(polySetNewDepthForTranslucent))));\n\
		outPolyID = vec4(float(polyID)/63.0, 0.0, 0.0, float(newFragColor.a > 0.999));\n\
		outFogAttributes = vec4( float(polyEnableFog), 0.0, 0.0, float(newFragColor.a > 0.999 || !bool(polyEnableFog)));\n\
		gl_FragDepth = newFragDepth;\n\
	} \n\
"};

// Vertex shader for applying edge marking, GLSL 1.50
static const char *EdgeMarkVtxShader_150 = {"\
	#version 150\n\
	\n\
	in vec2 inPosition;\n\
	in vec2 inTexCoord0;\n\
	layout (std140) uniform RenderStates\n\
	{\n\
		vec2 framebufferSize;\n\
		int toonShadingMode;\n\
		bool enableAlphaTest;\n\
		bool enableAntialiasing;\n\
		bool enableEdgeMarking;\n\
		bool enableFogAlphaOnly;\n\
		bool useWDepth;\n\
		float alphaTestRef;\n\
		float fogOffset;\n\
		float fogStep;\n\
		float pad_0;\n\
		vec4 fogColor;\n\
		float fogDensity[32];\n\
		vec4 edgeColor[8];\n\
		vec4 toonColor[32];\n\
	} state;\n\
	\n\
	out vec2 texCoord[5];\n\
	\n\
	void main()\n\
	{\n\
		vec2 texInvScale = vec2(1.0/state.framebufferSize.x, 1.0/state.framebufferSize.y);\n\
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
	layout (std140) uniform RenderStates\n\
	{\n\
		vec2 framebufferSize;\n\
		int toonShadingMode;\n\
		bool enableAlphaTest;\n\
		bool enableAntialiasing;\n\
		bool enableEdgeMarking;\n\
		bool enableFogAlphaOnly;\n\
		bool useWDepth;\n\
		float alphaTestRef;\n\
		float fogOffset;\n\
		float fogStep;\n\
		float pad_0;\n\
		vec4 fogColor;\n\
		float fogDensity[32];\n\
		vec4 edgeColor[8];\n\
		vec4 toonColor[32];\n\
	} state;\n\
	\n\
	uniform sampler2D texInFragDepth;\n\
	uniform sampler2D texInPolyID;\n\
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
		int polyID[5];\n\
		polyID[0] = int((texture(texInPolyID, texCoord[0]).r * 63.0) + 0.5);\n\
		polyID[1] = int((texture(texInPolyID, texCoord[1]).r * 63.0) + 0.5);\n\
		polyID[2] = int((texture(texInPolyID, texCoord[2]).r * 63.0) + 0.5);\n\
		polyID[3] = int((texture(texInPolyID, texCoord[3]).r * 63.0) + 0.5);\n\
		polyID[4] = int((texture(texInPolyID, texCoord[4]).r * 63.0) + 0.5);\n\
		\n\
		float depth[5];\n\
		depth[0] = unpackFloatFromVec3(texture(texInFragDepth, texCoord[0]).rgb);\n\
		depth[1] = unpackFloatFromVec3(texture(texInFragDepth, texCoord[1]).rgb);\n\
		depth[2] = unpackFloatFromVec3(texture(texInFragDepth, texCoord[2]).rgb);\n\
		depth[3] = unpackFloatFromVec3(texture(texInFragDepth, texCoord[3]).rgb);\n\
		depth[4] = unpackFloatFromVec3(texture(texInFragDepth, texCoord[4]).rgb);\n\
		\n\
		vec4 newEdgeColor = vec4(0.0, 0.0, 0.0, 0.0);\n\
		\n\
		for (int i = 1; i < 5; i++)\n\
		{\n\
			if (polyID[0] != polyID[i] && depth[0] >= depth[i])\n\
			{\n\
				newEdgeColor = state.edgeColor[polyID[i]/8];\n\
				break;\n\
			}\n\
		}\n\
		\n\
		outFragColor = newEdgeColor;\n\
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
	layout (std140) uniform RenderStates\n\
	{\n\
		vec2 framebufferSize;\n\
		int toonShadingMode;\n\
		bool enableAlphaTest;\n\
		bool enableAntialiasing;\n\
		bool enableEdgeMarking;\n\
		bool enableFogAlphaOnly;\n\
		bool useWDepth;\n\
		float alphaTestRef;\n\
		float fogOffset;\n\
		float fogStep;\n\
		float pad_0;\n\
		vec4 fogColor;\n\
		float fogDensity[32];\n\
		vec4 edgeColor[8];\n\
		vec4 toonColor[32];\n\
	} state;\n\
	\n\
	uniform sampler2D texInFragColor;\n\
	uniform sampler2D texInFragDepth;\n\
	uniform sampler2D texInFogAttributes;\n\
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
			if (inFragDepth <= min(state.fogOffset + state.fogStep, 1.0))\n\
			{\n\
				fogMixWeight = state.fogDensity[0];\n\
			}\n\
			else if (inFragDepth >= min(state.fogOffset + (state.fogStep*32.0), 1.0))\n\
			{\n\
				fogMixWeight = state.fogDensity[31];\n\
			}\n\
			else\n\
			{\n\
				for (int i = 1; i < 32; i++)\n\
				{\n\
					float currentFogStep = min(state.fogOffset + (state.fogStep * float(i+1)), 1.0);\n\
					if (inFragDepth <= currentFogStep)\n\
					{\n\
						float previousFogStep = min(state.fogOffset + (state.fogStep * float(i)), 1.0);\n\
						fogMixWeight = mix(state.fogDensity[i-1], state.fogDensity[i], (inFragDepth - previousFogStep) / (currentFogStep - previousFogStep));\n\
						break;\n\
					}\n\
				}\n\
			}\n\
			\n\
			newFoggedColor = mix(inFragColor, (state.enableFogAlphaOnly) ? vec4(inFragColor.rgb, state.fogColor.a) : state.fogColor, fogMixWeight);\n\
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

Render3DError OpenGLRenderer_3_2::InitEdgeMarkProgramShaderLocations()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	const GLuint uniformBlockRenderStates = glGetUniformBlockIndex(OGLRef.programEdgeMarkID, "RenderStates");
	glUniformBlockBinding(OGLRef.programEdgeMarkID, uniformBlockRenderStates, OGLBindingPointID_RenderStates);
	
	const GLint uniformTexGDepth	= glGetUniformLocation(OGLRef.programEdgeMarkID, "texInFragDepth");
	const GLint uniformTexGPolyID	= glGetUniformLocation(OGLRef.programEdgeMarkID, "texInPolyID");
	glUniform1i(uniformTexGDepth, OGLTextureUnitID_GDepth);
	glUniform1i(uniformTexGPolyID, OGLTextureUnitID_GPolyID);
	
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

Render3DError OpenGLRenderer_3_2::InitFogProgramShaderLocations()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	const GLuint uniformBlockRenderStates = glGetUniformBlockIndex(OGLRef.programFogID, "RenderStates");
	glUniformBlockBinding(OGLRef.programFogID, uniformBlockRenderStates, OGLBindingPointID_RenderStates);
	
	const GLint uniformTexGColor	= glGetUniformLocation(OGLRef.programFogID, "texInFragColor");
	const GLint uniformTexGDepth	= glGetUniformLocation(OGLRef.programFogID, "texInFragDepth");
	const GLint uniformTexGFog		= glGetUniformLocation(OGLRef.programFogID, "texInFogAttributes");
	glUniform1i(uniformTexGColor, OGLTextureUnitID_GColor);
	glUniform1i(uniformTexGDepth, OGLTextureUnitID_GDepth);
	glUniform1i(uniformTexGFog, OGLTextureUnitID_FogAttr);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::CreateFBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up FBO render targets
	glGenTextures(1, &OGLRef.texCIColorID);
	glGenTextures(1, &OGLRef.texCIDepthID);
	glGenTextures(1, &OGLRef.texCIFogAttrID);
	glGenTextures(1, &OGLRef.texCIPolyID);
	glGenTextures(1, &OGLRef.texCIDepthStencilID);
	
	glGenTextures(1, &OGLRef.texGColorID);
	glGenTextures(1, &OGLRef.texGDepthID);
	glGenTextures(1, &OGLRef.texGFogAttrID);
	glGenTextures(1, &OGLRef.texGPolyID);
	glGenTextures(1, &OGLRef.texGDepthStencilID);
	glGenTextures(1, &OGLRef.texPostprocessFogID);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GColor);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthStencilID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, this->_framebufferWidth, this->_framebufferHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GDepth);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GPolyID);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGPolyID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FogAttr);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGFogAttrID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTexture(GL_TEXTURE0);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texPostprocessFogID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIDepthStencilID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIDepthID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIPolyID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIFogAttrID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	// Set up FBOs
	glGenFramebuffers(1, &OGLRef.fboClearImageID);
	glGenFramebuffers(1, &OGLRef.fboRenderID);
	glGenFramebuffers(1, &OGLRef.fboPostprocessID);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboClearImageID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, OGLRef.texCIColorID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, OGLRef.texCIDepthID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, OGLRef.texCIPolyID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, OGLRef.texCIFogAttrID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, OGLRef.texCIDepthStencilID, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to created FBOs. Some emulation features will be disabled.\n");
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &OGLRef.fboClearImageID);
		glDeleteFramebuffers(1, &OGLRef.fboRenderID);
		glDeleteFramebuffers(1, &OGLRef.fboPostprocessID);
		glDeleteTextures(1, &OGLRef.texCIColorID);
		glDeleteTextures(1, &OGLRef.texCIDepthID);
		glDeleteTextures(1, &OGLRef.texCIFogAttrID);
		glDeleteTextures(1, &OGLRef.texCIPolyID);
		glDeleteTextures(1, &OGLRef.texCIDepthStencilID);
		glDeleteTextures(1, &OGLRef.texGColorID);
		glDeleteTextures(1, &OGLRef.texGDepthID);
		glDeleteTextures(1, &OGLRef.texGPolyID);
		glDeleteTextures(1, &OGLRef.texGFogAttrID);
		glDeleteTextures(1, &OGLRef.texGDepthStencilID);
		glDeleteTextures(1, &OGLRef.texPostprocessFogID);
		
		OGLRef.fboClearImageID = 0;
		OGLRef.fboRenderID = 0;
		OGLRef.fboPostprocessID = 0;
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glDrawBuffers(4, RenderDrawList);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	
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
		glDeleteFramebuffers(1, &OGLRef.fboClearImageID);
		glDeleteFramebuffers(1, &OGLRef.fboRenderID);
		glDeleteFramebuffers(1, &OGLRef.fboPostprocessID);
		glDeleteTextures(1, &OGLRef.texCIColorID);
		glDeleteTextures(1, &OGLRef.texCIDepthID);
		glDeleteTextures(1, &OGLRef.texCIFogAttrID);
		glDeleteTextures(1, &OGLRef.texCIPolyID);
		glDeleteTextures(1, &OGLRef.texCIDepthStencilID);
		glDeleteTextures(1, &OGLRef.texGColorID);
		glDeleteTextures(1, &OGLRef.texGDepthID);
		glDeleteTextures(1, &OGLRef.texGPolyID);
		glDeleteTextures(1, &OGLRef.texGFogAttrID);
		glDeleteTextures(1, &OGLRef.texGDepthStencilID);
		glDeleteTextures(1, &OGLRef.texPostprocessFogID);
		
		OGLRef.fboClearImageID = 0;
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
		glDeleteFramebuffers(1, &OGLRef.fboClearImageID);
		glDeleteFramebuffers(1, &OGLRef.fboRenderID);
		glDeleteFramebuffers(1, &OGLRef.fboPostprocessID);
		glDeleteTextures(1, &OGLRef.texCIColorID);
		glDeleteTextures(1, &OGLRef.texCIDepthID);
		glDeleteTextures(1, &OGLRef.texCIFogAttrID);
		glDeleteTextures(1, &OGLRef.texCIPolyID);
		glDeleteTextures(1, &OGLRef.texCIDepthStencilID);
		glDeleteTextures(1, &OGLRef.texGColorID);
		glDeleteTextures(1, &OGLRef.texGDepthID);
		glDeleteTextures(1, &OGLRef.texGPolyID);
		glDeleteTextures(1, &OGLRef.texGFogAttrID);
		glDeleteTextures(1, &OGLRef.texGDepthStencilID);
		glDeleteTextures(1, &OGLRef.texPostprocessFogID);
		
		OGLRef.fboClearImageID = 0;
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
	glDeleteFramebuffers(1, &OGLRef.fboClearImageID);
	glDeleteFramebuffers(1, &OGLRef.fboRenderID);
	glDeleteFramebuffers(1, &OGLRef.fboPostprocessID);
	glDeleteTextures(1, &OGLRef.texCIColorID);
	glDeleteTextures(1, &OGLRef.texCIDepthID);
	glDeleteTextures(1, &OGLRef.texCIFogAttrID);
	glDeleteTextures(1, &OGLRef.texCIPolyID);
	glDeleteTextures(1, &OGLRef.texCIDepthStencilID);
	glDeleteTextures(1, &OGLRef.texGColorID);
	glDeleteTextures(1, &OGLRef.texGDepthID);
	glDeleteTextures(1, &OGLRef.texGPolyID);
	glDeleteTextures(1, &OGLRef.texGFogAttrID);
	glDeleteTextures(1, &OGLRef.texGDepthStencilID);
	glDeleteTextures(1, &OGLRef.texPostprocessFogID);
	
	OGLRef.fboClearImageID = 0;
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
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGDepthID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGPolyID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGFogAttrID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGDepthStencilID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_DEPTH24_STENCIL8, this->_framebufferWidth, this->_framebufferHeight);
	
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

Render3DError OpenGLRenderer_3_2::InitGeometryProgramShaderLocations()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up render states UBO
	const GLuint uniformBlockRenderStates	= glGetUniformBlockIndex(OGLRef.programGeometryID, "RenderStates");
	glUniformBlockBinding(OGLRef.programGeometryID, uniformBlockRenderStates, OGLBindingPointID_RenderStates);
	
	GLint uboSize = 0;
	glGetActiveUniformBlockiv(OGLRef.programGeometryID, uniformBlockRenderStates, GL_UNIFORM_BLOCK_DATA_SIZE, &uboSize);
	assert(uboSize == sizeof(OGLRenderStates));
	
	glGenBuffers(1, &OGLRef.uboRenderStatesID);
	glBindBuffer(GL_UNIFORM_BUFFER, OGLRef.uboRenderStatesID);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(OGLRenderStates), NULL, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, OGLBindingPointID_RenderStates, OGLRef.uboRenderStatesID);
	
	// Set up poly states TBO
	glGenBuffers(1, &OGLRef.tboPolyStatesID);
	glBindBuffer(GL_TEXTURE_BUFFER, OGLRef.tboPolyStatesID);
	glBufferData(GL_TEXTURE_BUFFER, POLYLIST_SIZE * sizeof(OGLPolyStates), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	
	glGenTextures(1, &OGLRef.texPolyStatesID);
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_PolyStates);
	glBindTexture(GL_TEXTURE_BUFFER, OGLRef.texPolyStatesID);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8UI, OGLRef.tboPolyStatesID);
	glActiveTexture(GL_TEXTURE0);
	
	const GLint uniformTexRenderObject		= glGetUniformLocation(OGLRef.programGeometryID, "texRenderObject");
	const GLint uniformTexBufferPolyStates	= glGetUniformLocation(OGLRef.programGeometryID, "PolyStates");
	glUniform1i(uniformTexRenderObject, 0);
	glUniform1i(uniformTexBufferPolyStates, OGLTextureUnitID_PolyStates);
	
	OGLRef.uniformPolyStateIndex			= glGetUniformLocation(OGLRef.programGeometryID, "polyIndex");
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_3_2::DestroyGeometryProgram()
{
	if(!this->isShaderSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glUseProgram(0);
	
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	glDeleteBuffers(1, &OGLRef.uboRenderStatesID);
	glDeleteBuffers(1, &OGLRef.tboPolyStatesID);
	
	glDetachShader(OGLRef.programGeometryID, OGLRef.vertexGeometryShaderID);
	glDetachShader(OGLRef.programGeometryID, OGLRef.fragmentGeometryShaderID);
	
	glDeleteProgram(OGLRef.programGeometryID);
	glDeleteShader(OGLRef.vertexGeometryShaderID);
	glDeleteShader(OGLRef.fragmentGeometryShaderID);
	
	this->DestroyToonTable();
	
	this->isShaderSupported = false;
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

Render3DError OpenGLRenderer_3_2::EnableVertexAttributes()
{
	glBindVertexArray(this->ref->vaoGeometryStatesID);
	glActiveTexture(GL_TEXTURE0);
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::DisableVertexAttributes()
{
	glBindVertexArray(0);
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
		glBlitFramebuffer(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the working depth buffer
		glReadBuffer(GL_COLOR_ATTACHMENT1);
		glDrawBuffer(GL_COLOR_ATTACHMENT1);
		glBlitFramebuffer(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the polygon ID buffer
		glReadBuffer(GL_COLOR_ATTACHMENT2);
		glDrawBuffer(GL_COLOR_ATTACHMENT2);
		glBlitFramebuffer(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the fog buffer
		glReadBuffer(GL_COLOR_ATTACHMENT3);
		glDrawBuffer(GL_COLOR_ATTACHMENT3);
		glBlitFramebuffer(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Reset framebuffer targets
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffers(4, RenderDrawList);
		glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboRenderID);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::BeginRender(const GFX3D &engine)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if(!BEGINGL())
	{
		return OGLERROR_BEGINGL_FAILED;
	}
	
	// Since glReadPixels() is called at the end of every render, we know that rendering
	// must be synchronized at that time. Therefore, GL_MAP_UNSYNCHRONIZED_BIT should be
	// safe to use.
	
	glBindBuffer(GL_UNIFORM_BUFFER, OGLRef.uboRenderStatesID);
	OGLRenderStates *state = (OGLRenderStates *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(OGLRenderStates), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
	
	state->framebufferSize.x = this->_framebufferWidth;
	state->framebufferSize.y = this->_framebufferHeight;
	state->toonShadingMode = engine.renderState.shading;
	state->enableAlphaTest = (engine.renderState.enableAlphaTest) ? GL_TRUE : GL_FALSE;
	state->enableAntialiasing = (engine.renderState.enableAntialiasing) ? GL_TRUE : GL_FALSE;
	state->enableEdgeMarking = (engine.renderState.enableEdgeMarking) ? GL_TRUE : GL_FALSE;
	state->enableFogAlphaOnly = (engine.renderState.enableFogAlphaOnly) ? GL_TRUE : GL_FALSE;
	state->useWDepth = (engine.renderState.wbuffer) ? GL_TRUE : GL_FALSE;
	state->alphaTestRef = divide5bitBy31_LUT[engine.renderState.alphaTestRef];
	state->fogColor.r = divide5bitBy31_LUT[(engine.renderState.fogColor      ) & 0x0000001F];
	state->fogColor.g = divide5bitBy31_LUT[(engine.renderState.fogColor >>  5) & 0x0000001F];
	state->fogColor.b = divide5bitBy31_LUT[(engine.renderState.fogColor >> 10) & 0x0000001F];
	state->fogColor.a = divide5bitBy31_LUT[(engine.renderState.fogColor >> 16) & 0x0000001F];
	state->fogOffset = (GLfloat)engine.renderState.fogOffset / 32767.0f;
	state->fogStep = (GLfloat)(0x0400 >> engine.renderState.fogShift) / 32767.0f;
	
	for (size_t i = 0; i < 32; i++)
	{
		state->fogDensity[i].r = (engine.renderState.fogDensityTable[i] == 127) ? 1.0f : (GLfloat)engine.renderState.fogDensityTable[i] / 128.0f;
		state->fogDensity[i].g = 0.0f;
		state->fogDensity[i].b = 0.0f;
		state->fogDensity[i].a = 0.0f;
	}
	
	const GLfloat edgeColorAlpha = (engine.renderState.enableAntialiasing) ? (16.0f/31.0f) : 1.0f;
	for (size_t i = 0; i < 8; i++)
	{
		state->edgeColor[i].r = divide5bitBy31_LUT[(engine.renderState.edgeMarkColorTable[i]      ) & 0x001F];
		state->edgeColor[i].g = divide5bitBy31_LUT[(engine.renderState.edgeMarkColorTable[i] >>  5) & 0x001F];
		state->edgeColor[i].b = divide5bitBy31_LUT[(engine.renderState.edgeMarkColorTable[i] >> 10) & 0x001F];
		state->edgeColor[i].a = edgeColorAlpha;
	}
	
	for (size_t i = 0; i < 32; i++)
	{
		state->toonColor[i].r = divide5bitBy31_LUT[(engine.renderState.u16ToonTable[i]      ) & 0x001F];
		state->toonColor[i].g = divide5bitBy31_LUT[(engine.renderState.u16ToonTable[i] >>  5) & 0x001F];
		state->toonColor[i].b = divide5bitBy31_LUT[(engine.renderState.u16ToonTable[i] >> 10) & 0x001F];
		state->toonColor[i].a = 1.0f;
	}
	
	glUnmapBuffer(GL_UNIFORM_BUFFER);
	
	// Do per-poly setup
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_PolyStates);
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
	glBindBuffer(GL_TEXTURE_BUFFER, OGLRef.tboPolyStatesID);
	
	size_t vertIndexCount = 0;
	GLushort *indexPtr = (GLushort *)glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, engine.polylist->count * 6 * sizeof(GLushort), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
	OGLPolyStates *polyStates = (OGLPolyStates *)glMapBufferRange(GL_TEXTURE_BUFFER, 0, engine.polylist->count * sizeof(OGLPolyStates), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
	
	for (size_t i = 0; i < engine.polylist->count; i++)
	{
		const POLY *thePoly = &engine.polylist->list[engine.indexlist.list[i]];
		const size_t polyType = thePoly->type;
		PolygonAttributes polyAttr = thePoly->getAttributes();
		PolygonTexParams texParams = thePoly->getTexParams();
		
		polyStates[i].enableTexture = (texParams.texFormat != TEXMODE_NONE && engine.renderState.enableTexturing) ? GL_TRUE : GL_FALSE;
		polyStates[i].enableFog = (polyAttr.enableRenderFog) ? GL_TRUE : GL_FALSE;
		polyStates[i].enableDepthWrite = ((!polyAttr.isTranslucent || polyAttr.enableAlphaDepthWrite) && !(polyAttr.polygonMode == POLYGON_MODE_SHADOW && polyAttr.polygonID == 0)) ? GL_TRUE : GL_FALSE;
		polyStates[i].setNewDepthForTranslucent = (polyAttr.enableAlphaDepthWrite) ? GL_TRUE : GL_FALSE;
		polyStates[i].polyAlpha = (!polyAttr.isWireframe && polyAttr.isTranslucent) ? polyAttr.alpha : 0x1F;
		polyStates[i].polyMode = polyAttr.polygonMode;
		polyStates[i].polyID = polyAttr.polygonID;
		polyStates[i].texSizeS = texParams.sizeS;
		polyStates[i].texSizeT = texParams.sizeT;
		
		for (size_t j = 0; j < polyType; j++)
		{
			const GLushort vertIndex = thePoly->vertIndexes[j];
			
			// While we're looping through our vertices, add each vertex index to
			// a buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
			// vertices here to convert them to GL_TRIANGLES, which are much easier
			// to work with and won't be deprecated in future OpenGL versions.
			indexPtr[vertIndexCount++] = vertIndex;
			if (thePoly->vtxFormat == GFX3D_QUADS || thePoly->vtxFormat == GFX3D_QUAD_STRIP)
			{
				if (j == 2)
				{
					indexPtr[vertIndexCount++] = vertIndex;
				}
				else if (j == 3)
				{
					indexPtr[vertIndexCount++] = thePoly->vertIndexes[0];
				}
			}
		}
	}
	
	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	glUnmapBuffer(GL_TEXTURE_BUFFER);
	
	const size_t vtxBufferSize = sizeof(VERT) * engine.vertlist->count;
	VERT *vtxPtr = (VERT *)glMapBufferRange(GL_ARRAY_BUFFER, 0, vtxBufferSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
	memcpy(vtxPtr, engine.vertlist, vtxBufferSize);
	glUnmapBuffer(GL_ARRAY_BUFFER);
	
	glUseProgram(OGLRef.programGeometryID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::RenderEdgeMarking(const u16 *colorTable, const bool useAntialias)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboRenderID);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glUseProgram(OGLRef.programEdgeMarkID);
	
	glViewport(0, 0, this->_framebufferWidth, this->_framebufferHeight);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboPostprocessIndexID);
	glBindVertexArray(OGLRef.vaoPostprocessStatesID);
	
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
	
	glBindVertexArray(0);
	
	return RENDER3DERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::RenderFog(const u8 *densityTable, const u32 color, const u32 offset, const u8 shift, const bool alphaOnly)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboPostprocessID);
	glUseProgram(OGLRef.programFogID);
	
	glViewport(0, 0, this->_framebufferWidth, this->_framebufferHeight);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboPostprocessIndexID);
	glBindVertexArray(OGLRef.vaoPostprocessStatesID);
	
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
	
	glBindVertexArray(0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::CreateToonTable()
{
	// Do nothing. The toon table is updated in the render states UBO.
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::DestroyToonTable()
{
	// Do nothing. The toon table is updated in the render states UBO.
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::UpdateToonTable(const u16 *toonTableBuffer)
{
	// Do nothing. The toon table is updated in the render states UBO.
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const bool *__restrict fogBuffer, const u8 *__restrict polyIDBuffer)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	this->UploadClearImage(colorBuffer, depthBuffer, fogBuffer, polyIDBuffer);
	
	glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.fboClearImageID);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.fboRenderID);
	
	// Blit the working depth buffer
	glReadBuffer(GL_COLOR_ATTACHMENT1);
	glDrawBuffer(GL_COLOR_ATTACHMENT1);
	glBlitFramebuffer(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	
	// Blit the polygon ID buffer
	glReadBuffer(GL_COLOR_ATTACHMENT2);
	glDrawBuffer(GL_COLOR_ATTACHMENT2);
	glBlitFramebuffer(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	
	// Blit the fog buffer
	glReadBuffer(GL_COLOR_ATTACHMENT3);
	glDrawBuffer(GL_COLOR_ATTACHMENT3);
	glBlitFramebuffer(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	
	// Blit the color buffer. Do this last so that color attachment 0 is set to the read FBO.
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glBlitFramebuffer(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboRenderID);
	glDrawBuffers(4, RenderDrawList);
	
	OGLRef.selectedRenderingFBO = (CommonSettings.GFX3D_Renderer_Multisample) ? OGLRef.fboMSIntermediateRenderID : OGLRef.fboRenderID;
	if (OGLRef.selectedRenderingFBO == OGLRef.fboMSIntermediateRenderID)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.fboRenderID);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
		
		// Blit the working depth buffer
		glReadBuffer(GL_COLOR_ATTACHMENT1);
		glDrawBuffer(GL_COLOR_ATTACHMENT1);
		glBlitFramebuffer(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the polygon ID buffer
		glReadBuffer(GL_COLOR_ATTACHMENT2);
		glDrawBuffer(GL_COLOR_ATTACHMENT2);
		glBlitFramebuffer(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the fog buffer
		glReadBuffer(GL_COLOR_ATTACHMENT3);
		glDrawBuffer(GL_COLOR_ATTACHMENT3);
		glBlitFramebuffer(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the color buffer. Do this last so that color attachment 0 is set to the read FBO.
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
		
		glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
		glDrawBuffers(4, RenderDrawList);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::ClearUsingValues(const FragmentColor &clearColor, const FragmentAttributes &clearAttributes) const
{
	OGLRenderRef &OGLRef = *this->ref;
	OGLRef.selectedRenderingFBO = (CommonSettings.GFX3D_Renderer_Multisample) ? OGLRef.fboMSIntermediateRenderID : OGLRef.fboRenderID;
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
	glDrawBuffers(4, RenderDrawList);
	glDepthMask(GL_TRUE);
	
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

void OpenGLRenderer_3_2::SetPolygonIndex(const size_t index)
{
	this->_currentPolyIndex = index;
	glUniform1i(this->ref->uniformPolyStateIndex, index);
}

Render3DError OpenGLRenderer_3_2::SetupPolygon(const POLY &thePoly)
{
	const PolygonAttributes attr = thePoly.getAttributes();
	
	// Set up depth test mode
	static const GLenum oglDepthFunc[2] = {GL_LESS, GL_EQUAL};
	glDepthFunc(oglDepthFunc[attr.enableDepthEqualTest]);
	
	// Set up culling mode
	static const GLenum oglCullingMode[4] = {GL_FRONT_AND_BACK, GL_FRONT, GL_BACK, 0};
	GLenum cullingMode = oglCullingMode[attr.surfaceCullingMode];
	
	if (cullingMode == 0)
	{
		glDisable(GL_CULL_FACE);
	}
	else
	{
		glEnable(GL_CULL_FACE);
		glCullFace(cullingMode);
	}
	
	// Set up depth write
	GLboolean enableDepthWrite = GL_TRUE;
	
	// Handle shadow polys. Do this after checking for depth write, since shadow polys
	// can change this too.
	if (attr.polygonMode == POLYGON_MODE_SHADOW)
	{
		if(attr.polygonID == 0)
		{
			//when the polyID is zero, we are writing the shadow mask.
			//set stencilbuf = 1 where the shadow volume is obstructed by geometry.
			//do not write color or depth information.
			glStencilFunc(GL_ALWAYS, 65, 255);
			glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			enableDepthWrite = GL_FALSE;
		}
		else
		{
			//when the polyid is nonzero, we are drawing the shadow poly.
			//only draw the shadow poly where the stencilbuf==1.
			//I am not sure whether to update the depth buffer here--so I chose not to.
			glStencilFunc(GL_EQUAL, 65, 255);
			glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			enableDepthWrite = GL_TRUE;
		}
	}
	else
	{
		if (attr.isTranslucent)
		{
			glStencilFunc(GL_NOTEQUAL, attr.polygonID, 255);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		else
		{
			glStencilFunc(GL_ALWAYS, 64, 255);
			glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
	}
	
	if (attr.isTranslucent && !attr.enableAlphaDepthWrite)
	{
		enableDepthWrite = GL_FALSE;
	}
	
	glDepthMask(enableDepthWrite);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::SetupTexture(const POLY &thePoly, bool enableTexturing)
{
	OGLRenderRef &OGLRef = *this->ref;
	const PolygonTexParams params = thePoly.getTexParams();
	
	// Check if we need to use textures
	if (params.texFormat == TEXMODE_NONE || !enableTexturing)
	{
		return OGLERROR_NOERR;
	}
	
	TexCacheItem *newTexture = TexCache_SetTexture(TexFormat_32bpp, thePoly.texParam, thePoly.texPalette);
	if(newTexture != this->currTexture)
	{
		this->currTexture = newTexture;
		//has the ogl renderer initialized the texture?
		if(this->currTexture->GetDeleteCallback() == NULL)
		{
			this->currTexture->SetDeleteCallback(&texDeleteCallback, this, NULL);
			
			if(OGLRef.freeTextureIDs.empty())
			{
				this->ExpandFreeTextures();
			}
			
			this->currTexture->texid = (u64)OGLRef.freeTextureIDs.front();
			OGLRef.freeTextureIDs.pop();
			
			glBindTexture(GL_TEXTURE_2D, (GLuint)this->currTexture->texid);
			
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (params.enableRepeatS ? (params.enableMirroredRepeatS ? GL_MIRRORED_REPEAT : GL_REPEAT) : GL_CLAMP_TO_EDGE));
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (params.enableRepeatT ? (params.enableMirroredRepeatT ? GL_MIRRORED_REPEAT : GL_REPEAT) : GL_CLAMP_TO_EDGE));
			
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
						 this->currTexture->sizeX, this->currTexture->sizeY, 0,
						 GL_RGBA, GL_UNSIGNED_BYTE, this->currTexture->decoded);
		}
		else
		{
			//otherwise, just bind it
			glBindTexture(GL_TEXTURE_2D, (GLuint)this->currTexture->texid);
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::SetFramebufferSize(size_t w, size_t h)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (w < GFX3D_FRAMEBUFFER_WIDTH || h < GFX3D_FRAMEBUFFER_HEIGHT)
	{
		return OGLERROR_NOERR;
	}
	
	this->_framebufferWidth = w;
	this->_framebufferHeight = h;
	this->_framebufferColorSizeBytes = w * h * sizeof(FragmentColor);
	this->_framebufferColor = (FragmentColor *)realloc(this->_framebufferColor, this->_framebufferColorSizeBytes);
	
	if (oglrender_framebufferDidResizeCallback != NULL)
	{
		oglrender_framebufferDidResizeCallback(w, h);
	}
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GColor);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthStencilID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, w, h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGColorID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GDepth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GPolyID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FogAttr);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texPostprocessFogID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	if (this->isMultisampledFBOSupported)
	{
		GLint maxSamples = 0;
		glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
		
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGColorID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_RGBA, w, h);
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGDepthID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_RGBA, w, h);
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGPolyID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_RGBA, w, h);
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGFogAttrID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_RGBA, w, h);
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGDepthStencilID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_DEPTH24_STENCIL8, w, h);
	}
	
	glBufferData(GL_PIXEL_PACK_BUFFER, this->_framebufferColorSizeBytes, NULL, GL_STREAM_READ);
	
	return OGLERROR_NOERR;
}
