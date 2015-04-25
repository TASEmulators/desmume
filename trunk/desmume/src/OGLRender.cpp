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

#include "OGLRender.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "gfx3d.h"
#include "NDSSystem.h"
#include "texcache.h"


typedef struct
{
	unsigned int major;
	unsigned int minor;
	unsigned int revision;
} OGLVersion;

static OGLVersion _OGLDriverVersion = {0, 0, 0};
static OpenGLRenderer *_OGLRenderer = NULL;

// Lookup Tables
static CACHE_ALIGN GLfloat material_8bit_to_float[256] = {0};
CACHE_ALIGN const GLfloat divide5bitBy31_LUT[32]	= {0.0, 0.03225806451613, 0.06451612903226, 0.09677419354839,
													   0.1290322580645, 0.1612903225806, 0.1935483870968, 0.2258064516129,
													   0.2580645161290, 0.2903225806452, 0.3225806451613, 0.3548387096774,
													   0.3870967741935, 0.4193548387097, 0.4516129032258, 0.4838709677419,
													   0.5161290322581, 0.5483870967742, 0.5806451612903, 0.6129032258065,
													   0.6451612903226, 0.6774193548387, 0.7096774193548, 0.7419354838710,
													   0.7741935483871, 0.8064516129032, 0.8387096774194, 0.8709677419355,
													   0.9032258064516, 0.9354838709677, 0.9677419354839, 1.0};

const GLfloat PostprocessVtxBuffer[16]	= {-1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,
										    0.0f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f,  0.0f,  1.0f};
const GLubyte PostprocessElementBuffer[6] = {0, 1, 2, 2, 3, 0};

const GLenum RenderDrawList[4] = {GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_COLOR_ATTACHMENT3_EXT};

static bool BEGINGL()
{
	if(oglrender_beginOpenGL) 
		return oglrender_beginOpenGL();
	else return true;
}

static void ENDGL()
{
	if(oglrender_endOpenGL) 
		oglrender_endOpenGL();
}

// Function Pointers
bool (*oglrender_init)() = NULL;
bool (*oglrender_beginOpenGL)() = NULL;
void (*oglrender_endOpenGL)() = NULL;
void (*OGLLoadEntryPoints_3_2_Func)() = NULL;
void (*OGLCreateRenderer_3_2_Func)(OpenGLRenderer **rendererPtr) = NULL;

//------------------------------------------------------------

// Textures
#if !defined(GLX_H)
OGLEXT(PFNGLACTIVETEXTUREPROC, glActiveTexture) // Core in v1.3
OGLEXT(PFNGLACTIVETEXTUREARBPROC, glActiveTextureARB)
#endif

// Blending
OGLEXT(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate) // Core in v1.4
OGLEXT(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate) // Core in v2.0

OGLEXT(PFNGLBLENDFUNCSEPARATEEXTPROC, glBlendFuncSeparateEXT)
OGLEXT(PFNGLBLENDEQUATIONSEPARATEEXTPROC, glBlendEquationSeparateEXT)

// Shaders
OGLEXT(PFNGLCREATESHADERPROC, glCreateShader) // Core in v2.0
OGLEXT(PFNGLSHADERSOURCEPROC, glShaderSource) // Core in v2.0
OGLEXT(PFNGLCOMPILESHADERPROC, glCompileShader) // Core in v2.0
OGLEXT(PFNGLCREATEPROGRAMPROC, glCreateProgram) // Core in v2.0
OGLEXT(PFNGLATTACHSHADERPROC, glAttachShader) // Core in v2.0
OGLEXT(PFNGLDETACHSHADERPROC, glDetachShader) // Core in v2.0
OGLEXT(PFNGLLINKPROGRAMPROC, glLinkProgram) // Core in v2.0
OGLEXT(PFNGLUSEPROGRAMPROC, glUseProgram) // Core in v2.0
OGLEXT(PFNGLGETSHADERIVPROC, glGetShaderiv) // Core in v2.0
OGLEXT(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) // Core in v2.0
OGLEXT(PFNGLDELETESHADERPROC, glDeleteShader) // Core in v2.0
OGLEXT(PFNGLDELETEPROGRAMPROC, glDeleteProgram) // Core in v2.0
OGLEXT(PFNGLGETPROGRAMIVPROC, glGetProgramiv) // Core in v2.0
OGLEXT(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) // Core in v2.0
OGLEXT(PFNGLVALIDATEPROGRAMPROC, glValidateProgram) // Core in v2.0
OGLEXT(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) // Core in v2.0
OGLEXT(PFNGLUNIFORM1IPROC, glUniform1i) // Core in v2.0
OGLEXT(PFNGLUNIFORM1IVPROC, glUniform1iv) // Core in v2.0
OGLEXT(PFNGLUNIFORM1FPROC, glUniform1f) // Core in v2.0
OGLEXT(PFNGLUNIFORM1FVPROC, glUniform1fv) // Core in v2.0
OGLEXT(PFNGLUNIFORM2FPROC, glUniform2f) // Core in v2.0
OGLEXT(PFNGLUNIFORM4FPROC, glUniform4f) // Core in v2.0
OGLEXT(PFNGLUNIFORM4FVPROC, glUniform4fv) // Core in v2.0
OGLEXT(PFNGLDRAWBUFFERSPROC, glDrawBuffers) // Core in v2.0
OGLEXT(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) // Core in v2.0
OGLEXT(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) // Core in v2.0
OGLEXT(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) // Core in v2.0
OGLEXT(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) // Core in v2.0

// VAO
OGLEXT(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)
OGLEXT(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)
OGLEXT(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)

// Buffer Objects
OGLEXT(PFNGLGENBUFFERSARBPROC, glGenBuffersARB)
OGLEXT(PFNGLDELETEBUFFERSARBPROC, glDeleteBuffersARB)
OGLEXT(PFNGLBINDBUFFERARBPROC, glBindBufferARB)
OGLEXT(PFNGLBUFFERDATAARBPROC, glBufferDataARB)
OGLEXT(PFNGLBUFFERSUBDATAARBPROC, glBufferSubDataARB)
OGLEXT(PFNGLMAPBUFFERARBPROC, glMapBufferARB)
OGLEXT(PFNGLUNMAPBUFFERARBPROC, glUnmapBufferARB)

OGLEXT(PFNGLGENBUFFERSPROC, glGenBuffers) // Core in v1.5
OGLEXT(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) // Core in v1.5
OGLEXT(PFNGLBINDBUFFERPROC, glBindBuffer) // Core in v1.5
OGLEXT(PFNGLBUFFERDATAPROC, glBufferData) // Core in v1.5
OGLEXT(PFNGLBUFFERSUBDATAPROC, glBufferSubData) // Core in v1.5
OGLEXT(PFNGLMAPBUFFERPROC, glMapBuffer) // Core in v1.5
OGLEXT(PFNGLUNMAPBUFFERPROC, glUnmapBuffer) // Core in v1.5

// FBO
OGLEXT(PFNGLGENFRAMEBUFFERSEXTPROC, glGenFramebuffersEXT)
OGLEXT(PFNGLBINDFRAMEBUFFEREXTPROC, glBindFramebufferEXT)
OGLEXT(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC, glFramebufferRenderbufferEXT)
OGLEXT(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC, glFramebufferTexture2DEXT)
OGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, glCheckFramebufferStatusEXT)
OGLEXT(PFNGLDELETEFRAMEBUFFERSEXTPROC, glDeleteFramebuffersEXT)
OGLEXT(PFNGLBLITFRAMEBUFFEREXTPROC, glBlitFramebufferEXT)
OGLEXT(PFNGLGENRENDERBUFFERSEXTPROC, glGenRenderbuffersEXT)
OGLEXT(PFNGLBINDRENDERBUFFEREXTPROC, glBindRenderbufferEXT)
OGLEXT(PFNGLRENDERBUFFERSTORAGEEXTPROC, glRenderbufferStorageEXT)
OGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC, glRenderbufferStorageMultisampleEXT)
OGLEXT(PFNGLDELETERENDERBUFFERSEXTPROC, glDeleteRenderbuffersEXT)

static void OGLLoadEntryPoints_Legacy()
{
	// Textures
	#if !defined(GLX_H)
	INITOGLEXT(PFNGLACTIVETEXTUREPROC, glActiveTexture) // Core in v1.3
	INITOGLEXT(PFNGLACTIVETEXTUREARBPROC, glActiveTextureARB)
	#endif

	// Blending
	INITOGLEXT(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate) // Core in v1.4
	INITOGLEXT(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate) // Core in v2.0

	INITOGLEXT(PFNGLBLENDFUNCSEPARATEEXTPROC, glBlendFuncSeparateEXT)
	INITOGLEXT(PFNGLBLENDEQUATIONSEPARATEEXTPROC, glBlendEquationSeparateEXT)

	// Shaders
	INITOGLEXT(PFNGLCREATESHADERPROC, glCreateShader) // Core in v2.0
	INITOGLEXT(PFNGLSHADERSOURCEPROC, glShaderSource) // Core in v2.0
	INITOGLEXT(PFNGLCOMPILESHADERPROC, glCompileShader) // Core in v2.0
	INITOGLEXT(PFNGLCREATEPROGRAMPROC, glCreateProgram) // Core in v2.0
	INITOGLEXT(PFNGLATTACHSHADERPROC, glAttachShader) // Core in v2.0
	INITOGLEXT(PFNGLDETACHSHADERPROC, glDetachShader) // Core in v2.0
	INITOGLEXT(PFNGLLINKPROGRAMPROC, glLinkProgram) // Core in v2.0
	INITOGLEXT(PFNGLUSEPROGRAMPROC, glUseProgram) // Core in v2.0
	INITOGLEXT(PFNGLGETSHADERIVPROC, glGetShaderiv) // Core in v2.0
	INITOGLEXT(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) // Core in v2.0
	INITOGLEXT(PFNGLDELETESHADERPROC, glDeleteShader) // Core in v2.0
	INITOGLEXT(PFNGLDELETEPROGRAMPROC, glDeleteProgram) // Core in v2.0
	INITOGLEXT(PFNGLGETPROGRAMIVPROC, glGetProgramiv) // Core in v2.0
	INITOGLEXT(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) // Core in v2.0
	INITOGLEXT(PFNGLVALIDATEPROGRAMPROC, glValidateProgram) // Core in v2.0
	INITOGLEXT(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) // Core in v2.0
	INITOGLEXT(PFNGLUNIFORM1IPROC, glUniform1i) // Core in v2.0
	INITOGLEXT(PFNGLUNIFORM1IVPROC, glUniform1iv) // Core in v2.0
	INITOGLEXT(PFNGLUNIFORM1FPROC, glUniform1f) // Core in v2.0
	INITOGLEXT(PFNGLUNIFORM1FVPROC, glUniform1fv) // Core in v2.0
	INITOGLEXT(PFNGLUNIFORM2FPROC, glUniform2f) // Core in v2.0
	INITOGLEXT(PFNGLUNIFORM4FPROC, glUniform4f) // Core in v2.0
	INITOGLEXT(PFNGLUNIFORM4FVPROC, glUniform4fv) // Core in v2.0
	INITOGLEXT(PFNGLDRAWBUFFERSPROC, glDrawBuffers) // Core in v2.0
	INITOGLEXT(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) // Core in v2.0
	INITOGLEXT(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) // Core in v2.0
	INITOGLEXT(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) // Core in v2.0
	INITOGLEXT(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) // Core in v2.0

	// VAO
	INITOGLEXT(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)
	INITOGLEXT(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)
	INITOGLEXT(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)

	// Buffer Objects
	INITOGLEXT(PFNGLGENBUFFERSARBPROC, glGenBuffersARB)
	INITOGLEXT(PFNGLDELETEBUFFERSARBPROC, glDeleteBuffersARB)
	INITOGLEXT(PFNGLBINDBUFFERARBPROC, glBindBufferARB)
	INITOGLEXT(PFNGLBUFFERDATAARBPROC, glBufferDataARB)
	INITOGLEXT(PFNGLBUFFERSUBDATAARBPROC, glBufferSubDataARB)
	INITOGLEXT(PFNGLMAPBUFFERARBPROC, glMapBufferARB)
	INITOGLEXT(PFNGLUNMAPBUFFERARBPROC, glUnmapBufferARB)

	INITOGLEXT(PFNGLGENBUFFERSPROC, glGenBuffers) // Core in v1.5
	INITOGLEXT(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) // Core in v1.5
	INITOGLEXT(PFNGLBINDBUFFERPROC, glBindBuffer) // Core in v1.5
	INITOGLEXT(PFNGLBUFFERDATAPROC, glBufferData) // Core in v1.5
	INITOGLEXT(PFNGLBUFFERSUBDATAPROC, glBufferSubData) // Core in v1.5
	INITOGLEXT(PFNGLMAPBUFFERPROC, glMapBuffer) // Core in v1.5
	INITOGLEXT(PFNGLUNMAPBUFFERPROC, glUnmapBuffer) // Core in v1.5

	// FBO
	INITOGLEXT(PFNGLGENFRAMEBUFFERSEXTPROC, glGenFramebuffersEXT)
	INITOGLEXT(PFNGLBINDFRAMEBUFFEREXTPROC, glBindFramebufferEXT)
	INITOGLEXT(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC, glFramebufferRenderbufferEXT)
	INITOGLEXT(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC, glFramebufferTexture2DEXT)
	INITOGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, glCheckFramebufferStatusEXT)
	INITOGLEXT(PFNGLDELETEFRAMEBUFFERSEXTPROC, glDeleteFramebuffersEXT)
	INITOGLEXT(PFNGLBLITFRAMEBUFFEREXTPROC, glBlitFramebufferEXT)
	INITOGLEXT(PFNGLGENRENDERBUFFERSEXTPROC, glGenRenderbuffersEXT)
	INITOGLEXT(PFNGLBINDRENDERBUFFEREXTPROC, glBindRenderbufferEXT)
	INITOGLEXT(PFNGLRENDERBUFFERSTORAGEEXTPROC, glRenderbufferStorageEXT)
	INITOGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC, glRenderbufferStorageMultisampleEXT)
	INITOGLEXT(PFNGLDELETERENDERBUFFERSEXTPROC, glDeleteRenderbuffersEXT)
}

// Vertex Shader GLSL 1.00
static const char *vertexShader_100 = {"\
	attribute vec4 inPosition; \n\
	attribute vec2 inTexCoord0; \n\
	attribute vec3 inColor; \n\
	\n\
	uniform float polyAlpha; \n\
	uniform vec2 polyTexScale; \n\
	\n\
	varying vec4 vtxPosition; \n\
	varying vec2 vtxTexCoord; \n\
	varying vec4 vtxColor; \n\
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

// Fragment Shader GLSL 1.00
static const char *fragmentShader_100 = {"\
	varying vec4 vtxPosition; \n\
	varying vec2 vtxTexCoord; \n\
	varying vec4 vtxColor; \n\
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
	vec3 packVec3FromFloat(const float value)\n\
	{\n\
		float expandedValue = value * 16777215.0;\n\
		vec3 packedValue = vec3( fract(expandedValue/256.0), fract(((expandedValue/256.0) - fract(expandedValue/256.0)) / 256.0), fract(((expandedValue/65536.0) - fract(expandedValue/65536.0)) / 256.0) );\n\
		return packedValue;\n\
	}\n\
	\n\
	void main() \n\
	{ \n\
		vec4 mainTexColor = (polyEnableTexture) ? texture2D(texRenderObject, vtxTexCoord) : vec4(1.0, 1.0, 1.0, 1.0); \n\
		vec4 newFragColor = mainTexColor * vtxColor; \n\
		\n\
		if(polyMode == 1) \n\
		{ \n\
			newFragColor.rgb = (polyEnableTexture) ? mix(vtxColor.rgb, mainTexColor.rgb, mainTexColor.a) : vtxColor.rgb; \n\
			newFragColor.a = vtxColor.a; \n\
		} \n\
		else if(polyMode == 2) \n\
		{ \n\
			vec3 toonColor = vec3(texture1D(texToonTable, vtxColor.r).rgb); \n\
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
		gl_FragData[0] = newFragColor;\n\
		gl_FragData[3] = vec4( float(polyEnableFog), float(stateEnableFogAlphaOnly), 0.0, 1.0);\n\
		if (newFragColor.a > 0.999) gl_FragData[2] = vec4(float(polyID)/63.0, float(stateEnableAntialiasing), 0.0, 1.0);\n\
		if (polyEnableDepthWrite && (newFragColor.a > 0.999 || polySetNewDepthForTranslucent)) gl_FragData[1] = vec4( packVec3FromFloat(newFragDepth), 1.0);\n\
		gl_FragDepth = newFragDepth;\n\
	} \n\
"};

// Vertex shader for applying edge marking, GLSL 1.00
static const char *EdgeMarkVtxShader_100 = {"\
	attribute vec2 inPosition;\n\
	attribute vec2 inTexCoord0;\n\
	uniform vec2 framebufferSize;\n\
	varying vec2 texCoord[5];\n\
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

// Fragment shader for applying edge marking, GLSL 1.00
static const char *EdgeMarkFragShader_100 = {"\
	varying vec2 texCoord[5];\n\
	\n\
	uniform sampler2D texInFragColor;\n\
	uniform sampler2D texInFragDepth;\n\
	uniform sampler2D texInPolyID;\n\
	uniform vec4 stateEdgeColor[8];\n\
	\n\
	float unpackFloatFromVec3(const vec3 value)\n\
	{\n\
		const vec3 unpackRatio = vec3(256.0, 65536.0, 16777216.0);\n\
		return (dot(value, unpackRatio) / 16777215.0);\n\
	}\n\
	\n\
	void main()\n\
	{\n\
		vec4 inFragColor = texture2D(texInFragColor, texCoord[0]);\n\
		float inFragDepth = unpackFloatFromVec3(texture2D(texInFragDepth, texCoord[0]).rgb);\n\
		\n\
		vec4 inPolyIDAttributes[5];\n\
		inPolyIDAttributes[0] = texture2D(texInPolyID, texCoord[0]);\n\
		inPolyIDAttributes[1] = texture2D(texInPolyID, texCoord[1]);\n\
		inPolyIDAttributes[2] = texture2D(texInPolyID, texCoord[2]);\n\
		inPolyIDAttributes[3] = texture2D(texInPolyID, texCoord[3]);\n\
		inPolyIDAttributes[4] = texture2D(texInPolyID, texCoord[4]);\n\
		\n\
		int polyID[5];\n\
		polyID[0] = int((inPolyIDAttributes[0].r * 63.0) + 0.5);\n\
		polyID[1] = int((inPolyIDAttributes[1].r * 63.0) + 0.5);\n\
		polyID[2] = int((inPolyIDAttributes[2].r * 63.0) + 0.5);\n\
		polyID[3] = int((inPolyIDAttributes[3].r * 63.0) + 0.5);\n\
		polyID[4] = int((inPolyIDAttributes[4].r * 63.0) + 0.5);\n\
		\n\
		bool antialias[5];\n\
		antialias[0] = bool(inPolyIDAttributes[0].g);\n\
		antialias[1] = bool(inPolyIDAttributes[1].g);\n\
		antialias[2] = bool(inPolyIDAttributes[2].g);\n\
		antialias[3] = bool(inPolyIDAttributes[3].g);\n\
		antialias[4] = bool(inPolyIDAttributes[4].g);\n\
		\n\
		float depth[5];\n\
		depth[0] = unpackFloatFromVec3(texture2D(texInFragDepth, texCoord[0]).rgb);\n\
		depth[1] = unpackFloatFromVec3(texture2D(texInFragDepth, texCoord[1]).rgb);\n\
		depth[2] = unpackFloatFromVec3(texture2D(texInFragDepth, texCoord[2]).rgb);\n\
		depth[3] = unpackFloatFromVec3(texture2D(texInFragDepth, texCoord[3]).rgb);\n\
		depth[4] = unpackFloatFromVec3(texture2D(texInFragDepth, texCoord[4]).rgb);\n\
		\n\
		vec4 edgeColor = stateEdgeColor[polyID[0]/8];\n\
		bool doEdgeMark = false;\n\
		bool doAntialias = false;\n\
		\n\
		for (int i = 1; i < 5; i++)\n\
		{\n\
			if (polyID[0] != polyID[i] && depth[0] >= depth[i])\n\
			{\n\
				doEdgeMark = true;\n\
				edgeColor = stateEdgeColor[polyID[i]/8];\n\
				doAntialias = antialias[i];\n\
				break;\n\
			}\n\
		}\n\
		\n\
		gl_FragData[0] = (doEdgeMark) ? ((doAntialias) ? mix(edgeColor, inFragColor, 0.4) : edgeColor) : inFragColor;\n\
	}\n\
"};

// Vertex shader for applying fog, GLSL 1.00
static const char *FogVtxShader_100 = {"\
	attribute vec2 inPosition;\n\
	attribute vec2 inTexCoord0;\n\
	varying vec2 texCoord;\n\
	\n\
	void main() \n\
	{ \n\
		texCoord = inTexCoord0; \n\
		gl_Position = vec4(inPosition, 0.0, 1.0);\n\
	}\n\
"};

// Fragment shader for applying fog, GLSL 1.00
static const char *FogFragShader_100 = {"\
	varying vec2 texCoord;\n\
	\n\
	uniform sampler2D texInFragColor;\n\
	uniform sampler2D texInFragDepth;\n\
	uniform sampler2D texInFogAttributes;\n\
	uniform vec4 stateFogColor;\n\
	uniform float stateFogDensity[32];\n\
	uniform float stateFogOffset;\n\
	uniform float stateFogStep;\n\
	\n\
	float unpackFloatFromVec3(const vec3 value)\n\
	{\n\
		const vec3 unpackRatio = vec3(256.0, 65536.0, 16777216.0);\n\
		return (dot(value, unpackRatio) / 16777215.0);\n\
	}\n\
	\n\
	void main()\n\
	{\n\
		vec4 inFragColor = texture2D(texInFragColor, texCoord);\n\
		float inFragDepth = unpackFloatFromVec3(texture2D(texInFragDepth, texCoord).rgb);\n\
		vec4 inFogAttributes = texture2D(texInFogAttributes, texCoord);\n\
		\n\
		bool polyEnableFog = bool(inFogAttributes.r);\n\
		bool stateEnableFogAlphaOnly = bool(inFogAttributes.g) ;\n\
		\n\
		float fogMixWeight = 0.0;\n\
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
		vec4 newFoggedColor = mix(inFragColor, stateFogColor, fogMixWeight);\n\
		gl_FragData[0] = (polyEnableFog) ? ((stateEnableFogAlphaOnly) ? vec4(inFragColor.rgb, newFoggedColor.a) : newFoggedColor) : inFragColor;\n\
	}\n\
"};

FORCEINLINE u32 BGRA8888_32_To_RGBA6665_32(const u32 srcPix)
{
	const u32 dstPix = (srcPix >> 2) & 0x3F3F3F3F;
	
	return	 (dstPix & 0x0000FF00) << 16 |		// R
			 (dstPix & 0x00FF0000)       |		// G
			 (dstPix & 0xFF000000) >> 16 |		// B
			((dstPix >> 1) & 0x000000FF);		// A
}

FORCEINLINE u32 BGRA8888_32Rev_To_RGBA6665_32Rev(const u32 srcPix)
{
	const u32 dstPix = (srcPix >> 2) & 0x3F3F3F3F;
	
	return	 (dstPix & 0x00FF0000) >> 16 |		// R
			 (dstPix & 0x0000FF00)       |		// G
			 (dstPix & 0x000000FF) << 16 |		// B
			((dstPix >> 1) & 0xFF000000);		// A
}

bool IsVersionSupported(unsigned int checkVersionMajor, unsigned int checkVersionMinor, unsigned int checkVersionRevision)
{
	bool result = false;
	
	if ( (_OGLDriverVersion.major > checkVersionMajor) ||
		 (_OGLDriverVersion.major >= checkVersionMajor && _OGLDriverVersion.minor > checkVersionMinor) ||
		 (_OGLDriverVersion.major >= checkVersionMajor && _OGLDriverVersion.minor >= checkVersionMinor && _OGLDriverVersion.revision >= checkVersionRevision) )
	{
		result = true;
	}
	
	return result;
}

static void OGLGetDriverVersion(const char *oglVersionString,
								unsigned int *versionMajor,
								unsigned int *versionMinor,
								unsigned int *versionRevision)
{
	size_t versionStringLength = 0;
	
	if (oglVersionString == NULL)
	{
		return;
	}
	
	// First, check for the dot in the revision string. There should be at
	// least one present.
	const char *versionStrEnd = strstr(oglVersionString, ".");
	if (versionStrEnd == NULL)
	{
		return;
	}
	
	// Next, check for the space before the vendor-specific info (if present).
	versionStrEnd = strstr(oglVersionString, " ");
	if (versionStrEnd == NULL)
	{
		// If a space was not found, then the vendor-specific info is not present,
		// and therefore the entire string must be the version number.
		versionStringLength = strlen(oglVersionString);
	}
	else
	{
		// If a space was found, then the vendor-specific info is present,
		// and therefore the version number is everything before the space.
		versionStringLength = versionStrEnd - oglVersionString;
	}
	
	// Copy the version substring and parse it.
	char *versionSubstring = (char *)malloc(versionStringLength * sizeof(char));
	strncpy(versionSubstring, oglVersionString, versionStringLength);
	
	unsigned int major = 0;
	unsigned int minor = 0;
	unsigned int revision = 0;
	
	sscanf(versionSubstring, "%u.%u.%u", &major, &minor, &revision);
	
	free(versionSubstring);
	versionSubstring = NULL;
	
	if (versionMajor != NULL)
	{
		*versionMajor = major;
	}
	
	if (versionMinor != NULL)
	{
		*versionMinor = minor;
	}
	
	if (versionRevision != NULL)
	{
		*versionRevision = revision;
	}
}

static void texDeleteCallback(TexCacheItem *item)
{
	_OGLRenderer->DeleteTexture(item);
}

template<bool require_profile, bool enable_3_2>
static char OGLInit(void)
{
	char result = 0;
	Render3DError error = OGLERROR_NOERR;
	
	if(!oglrender_init)
		return result;
	if(!oglrender_init())
		return result;
	
	result = Default3D_Init();
	if (result == 0)
	{
		return result;
	}

	if(!BEGINGL())
	{
		INFO("OpenGL<%s,%s>: Could not initialize -- BEGINGL() failed.\n",require_profile?"force":"auto",enable_3_2?"3_2":"old");
		result = 0;
		return result;
	}
	
	// Get OpenGL info
	const char *oglVersionString = (const char *)glGetString(GL_VERSION);
	const char *oglVendorString = (const char *)glGetString(GL_VENDOR);
	const char *oglRendererString = (const char *)glGetString(GL_RENDERER);

	// Writing to gl_FragDepth causes the driver to fail miserably on systems equipped 
	// with a Intel G965 graphic card. Warn the user and fail gracefully.
	// http://forums.desmume.org/viewtopic.php?id=9286
	if(!strcmp(oglVendorString,"Intel") && strstr(oglRendererString,"965")) 
	{
		INFO("Incompatible graphic card detected. Disabling OpenGL support.\n");
		result = 0;
		return result;
	}
	
	// Check the driver's OpenGL version
	OGLGetDriverVersion(oglVersionString, &_OGLDriverVersion.major, &_OGLDriverVersion.minor, &_OGLDriverVersion.revision);
	
	if (!IsVersionSupported(OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MAJOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MINOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_REVISION))
	{
		INFO("OpenGL: Driver does not support OpenGL v%u.%u.%u or later. Disabling 3D renderer.\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
			 OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MAJOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MINOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_REVISION,
			 oglVersionString, oglVendorString, oglRendererString);
		
		result = 0;
		return result;
	}
	
	// Create new OpenGL rendering object
	if(enable_3_2)
	{
		if (OGLLoadEntryPoints_3_2_Func != NULL && OGLCreateRenderer_3_2_Func != NULL)
		{
			OGLLoadEntryPoints_3_2_Func();
			OGLLoadEntryPoints_Legacy(); //zero 04-feb-2013 - this seems to be necessary as well
			OGLCreateRenderer_3_2_Func(&_OGLRenderer);
		}
		else 
		{
			if(require_profile)
				return 0;
		}
	}
	
	// If the renderer doesn't initialize with OpenGL v3.2 or higher, fall back
	// to one of the lower versions.
	if (_OGLRenderer == NULL)
	{
		OGLLoadEntryPoints_Legacy();
		
		if (IsVersionSupported(2, 1, 0))
		{
			_OGLRenderer = new OpenGLRenderer_2_1;
			_OGLRenderer->SetVersion(2, 1, 0);
		}
		else if (IsVersionSupported(2, 0, 0))
		{
			_OGLRenderer = new OpenGLRenderer_2_0;
			_OGLRenderer->SetVersion(2, 0, 0);
		}
		else if (IsVersionSupported(1, 5, 0))
		{
			_OGLRenderer = new OpenGLRenderer_1_5;
			_OGLRenderer->SetVersion(1, 5, 0);
		}
		else if (IsVersionSupported(1, 4, 0))
		{
			_OGLRenderer = new OpenGLRenderer_1_4;
			_OGLRenderer->SetVersion(1, 4, 0);
		}
		else if (IsVersionSupported(1, 3, 0))
		{
			_OGLRenderer = new OpenGLRenderer_1_3;
			_OGLRenderer->SetVersion(1, 3, 0);
		}
		else if (IsVersionSupported(1, 2, 0))
		{
			_OGLRenderer = new OpenGLRenderer_1_2;
			_OGLRenderer->SetVersion(1, 2, 0);
		}
	}
	
	if (_OGLRenderer == NULL)
	{
		INFO("OpenGL: Renderer did not initialize. Disabling 3D renderer.\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
			 oglVersionString, oglVendorString, oglRendererString);
		result = 0;
		return result;
	}
	
	// Initialize OpenGL extensions
	error = _OGLRenderer->InitExtensions();
	if (error != OGLERROR_NOERR)
	{
		if ( IsVersionSupported(2, 0, 0) &&
			(error == OGLERROR_SHADER_CREATE_ERROR ||
			 error == OGLERROR_VERTEX_SHADER_PROGRAM_LOAD_ERROR ||
			 error == OGLERROR_FRAGMENT_SHADER_PROGRAM_LOAD_ERROR) )
		{
			INFO("OpenGL: Shaders are not working, even though they should be. Disabling 3D renderer.\n");
			result = 0;
			return result;
		}
		else if (IsVersionSupported(3, 0, 0) && error == OGLERROR_FBO_CREATE_ERROR && OGLLoadEntryPoints_3_2_Func != NULL)
		{
			INFO("OpenGL: FBOs are not working, even though they should be. Disabling 3D renderer.\n");
			result = 0;
			return result;
		}
	}
	
	// Initialization finished -- reset the renderer
	_OGLRenderer->Reset();
	
	ENDGL();
	
	unsigned int major = 0;
	unsigned int minor = 0;
	unsigned int revision = 0;
	_OGLRenderer->GetVersion(&major, &minor, &revision);
	
	INFO("OpenGL: Renderer initialized successfully (v%u.%u.%u).\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
		 major, minor, revision, oglVersionString, oglVendorString, oglRendererString);
	
	return result;
}

static void OGLReset()
{
	if(!BEGINGL())
		return;
	
	_OGLRenderer->Reset();
	
	ENDGL();
}

static void OGLClose()
{
	if(!BEGINGL())
		return;
	
	delete _OGLRenderer;
	_OGLRenderer = NULL;
	
	ENDGL();
	
	Default3D_Close();
}

static void OGLRender()
{
	if(!BEGINGL())
		return;
	
	_OGLRenderer->Render(&gfx3d.renderState, gfx3d.vertlist, gfx3d.polylist, &gfx3d.indexlist, gfx3d.frameCtr);
	
	ENDGL();
}

static void OGLVramReconfigureSignal()
{
	if(!BEGINGL())
		return;
	
	_OGLRenderer->VramReconfigureSignal();
	
	ENDGL();
}

static void OGLRenderFinish()
{
	if(!BEGINGL())
		return;
	
	_OGLRenderer->RenderFinish();
	
	ENDGL();
}

//automatically select 3.2 or old profile depending on whether 3.2 is available
GPU3DInterface gpu3Dgl = {
	"OpenGL",
	OGLInit<false,true>,
	OGLReset,
	OGLClose,
	OGLRender,
	OGLRenderFinish,
	OGLVramReconfigureSignal
};

//forcibly use old profile
GPU3DInterface gpu3DglOld = {
	"OpenGL Old",
	OGLInit<true,false>,
	OGLReset,
	OGLClose,
	OGLRender,
	OGLRenderFinish,
	OGLVramReconfigureSignal
};

//forcibly use new profile
GPU3DInterface gpu3Dgl_3_2 = {
	"OpenGL 3.2",
	OGLInit<true,true>,
	OGLReset,
	OGLClose,
	OGLRender,
	OGLRenderFinish,
	OGLVramReconfigureSignal
};

OpenGLRenderer::OpenGLRenderer()
{
	versionMajor = 0;
	versionMinor = 0;
	versionRevision = 0;
}

bool OpenGLRenderer::IsExtensionPresent(const std::set<std::string> *oglExtensionSet, const std::string extensionName) const
{
	if (oglExtensionSet == NULL || oglExtensionSet->size() == 0)
	{
		return false;
	}
	
	return (oglExtensionSet->find(extensionName) != oglExtensionSet->end());
}

bool OpenGLRenderer::ValidateShaderCompile(GLuint theShader) const
{
	bool isCompileValid = false;
	GLint status = GL_FALSE;
	
	glGetShaderiv(theShader, GL_COMPILE_STATUS, &status);
	if(status == GL_TRUE)
	{
		isCompileValid = true;
	}
	else
	{
		GLint logSize;
		GLchar *log = NULL;
		
		glGetShaderiv(theShader, GL_INFO_LOG_LENGTH, &logSize);
		log = new GLchar[logSize];
		glGetShaderInfoLog(theShader, logSize, &logSize, log);
		
		INFO("OpenGL: SEVERE - FAILED TO COMPILE SHADER : %s\n", log);
		delete[] log;
	}
	
	return isCompileValid;
}

bool OpenGLRenderer::ValidateShaderProgramLink(GLuint theProgram) const
{
	bool isLinkValid = false;
	GLint status = GL_FALSE;
	
	glGetProgramiv(theProgram, GL_LINK_STATUS, &status);
	if(status == GL_TRUE)
	{
		isLinkValid = true;
	}
	else
	{
		GLint logSize;
		GLchar *log = NULL;
		
		glGetProgramiv(theProgram, GL_INFO_LOG_LENGTH, &logSize);
		log = new GLchar[logSize];
		glGetProgramInfoLog(theProgram, logSize, &logSize, log);
		
		INFO("OpenGL: SEVERE - FAILED TO LINK SHADER PROGRAM : %s\n", log);
		delete[] log;
	}
	
	return isLinkValid;
}

void OpenGLRenderer::GetVersion(unsigned int *major, unsigned int *minor, unsigned int *revision) const
{
	*major = this->versionMajor;
	*minor = this->versionMinor;
	*revision = this->versionRevision;
}

void OpenGLRenderer::SetVersion(unsigned int major, unsigned int minor, unsigned int revision)
{
	this->versionMajor = major;
	this->versionMinor = minor;
	this->versionRevision = revision;
}

void OpenGLRenderer::ConvertFramebuffer(const u32 *__restrict srcBuffer, u32 *dstBuffer)
{
	if (srcBuffer == NULL || dstBuffer == NULL)
	{
		return;
	}
	
	// Convert from 32-bit BGRA8888 format to 32-bit RGBA6665 reversed format. OpenGL
	// stores pixels using a flipped Y-coordinate, so this needs to be flipped back
	// to the DS Y-coordinate.
	for(int i = 0, y = 191; y >= 0; y--)
	{
		u32 *__restrict dst = dstBuffer + (y * GFX3D_FRAMEBUFFER_WIDTH);
		
		for(size_t x = 0; x < GFX3D_FRAMEBUFFER_WIDTH; x++, i++)
		{
			// Use the correct endian format since OpenGL uses the native endian of
			// the architecture it is running on.
#ifdef WORDS_BIGENDIAN
			*dst++ = BGRA8888_32_To_RGBA6665_32(srcBuffer[i]);
#else
			*dst++ = BGRA8888_32Rev_To_RGBA6665_32Rev(srcBuffer[i]);
#endif
		}
	}
}

OpenGLRenderer_1_2::OpenGLRenderer_1_2()
{
	isVBOSupported = false;
	isPBOSupported = false;
	isFBOSupported = false;
	isMultisampledFBOSupported = false;
	isShaderSupported = false;
	isVAOSupported = false;
	
	// Init OpenGL rendering states
	ref = new OGLRenderRef;
	ref->fboRenderID = 0;
	ref->fboMSIntermediateRenderID = 0;
	ref->fboPostprocessID = 0;
	ref->selectedRenderingFBO = 0;
}

OpenGLRenderer_1_2::~OpenGLRenderer_1_2()
{
	if (ref == NULL)
	{
		return;
	}
	
	glFinish();
	
	gpuScreen3DHasNewData[0] = false;
	gpuScreen3DHasNewData[1] = false;
	
	delete[] ref->color4fBuffer;
	ref->color4fBuffer = NULL;
	
	DestroyGeometryProgram();
	DestroyPostprocessingPrograms();
	DestroyVAOs();
	DestroyVBOs();
	DestroyPBOs();
	DestroyFBOs();
	DestroyMultisampledFBO();
	
	//kill the tex cache to free all the texture ids
	TexCache_Reset();
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	while(!ref->freeTextureIDs.empty())
	{
		GLuint temp = ref->freeTextureIDs.front();
		ref->freeTextureIDs.pop();
		glDeleteTextures(1, &temp);
	}
	
	glFinish();
	
	// Destroy OpenGL rendering states
	delete ref;
	ref = NULL;
}

Render3DError OpenGLRenderer_1_2::InitExtensions()
{
	Render3DError error = OGLERROR_NOERR;
	
	// Get OpenGL extensions
	std::set<std::string> oglExtensionSet;
	this->GetExtensionSet(&oglExtensionSet);
	
	// Initialize OpenGL
	this->InitTables();
	
	this->isShaderSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_shader_objects") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_shader") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_fragment_shader") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_program");
	if (this->isShaderSupported)
	{
		std::string vertexShaderProgram;
		std::string fragmentShaderProgram;
		
		error = this->LoadGeometryShaders(vertexShaderProgram, fragmentShaderProgram);
		if (error == OGLERROR_NOERR)
		{
			error = this->InitGeometryProgram(vertexShaderProgram, fragmentShaderProgram);
			if (error != OGLERROR_NOERR)
			{
				this->isShaderSupported = false;
			}
			
			std::string edgeMarkVtxShaderString = std::string(EdgeMarkVtxShader_100);
			std::string edgeMarkFragShaderString = std::string(EdgeMarkFragShader_100);
			std::string fogVtxShaderString = std::string(FogVtxShader_100);
			std::string fogFragShaderString = std::string(FogFragShader_100);
			error = this->InitPostprocessingPrograms(edgeMarkVtxShaderString, edgeMarkFragShaderString, fogVtxShaderString, fogFragShaderString);
			if (error != OGLERROR_NOERR)
			{
				this->DestroyGeometryProgram();
				this->isShaderSupported = false;
			}
		}
		else
		{
			this->isShaderSupported = false;
		}
	}
	else
	{
		INFO("OpenGL: Shaders are unsupported. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
	}
	
	this->isVBOSupported = this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_buffer_object");
	if (this->isVBOSupported)
	{
		this->CreateVBOs();
	}
	
	this->isPBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_buffer_object") &&
							 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_pixel_buffer_object") ||
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_pixel_buffer_object"));
	if (this->isPBOSupported)
	{
		this->CreatePBOs();
	}
	
	this->isVAOSupported	= this->isShaderSupported &&
							  this->isVBOSupported &&
							 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_array_object") ||
							  this->IsExtensionPresent(&oglExtensionSet, "GL_APPLE_vertex_array_object"));
	if (this->isVAOSupported)
	{
		this->CreateVAOs();
	}
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
	this->isFBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_object") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_blit") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_packed_depth_stencil");
	if (this->isFBOSupported)
	{
		error = this->CreateFBOs();
		if (error != OGLERROR_NOERR)
		{
			this->isFBOSupported = false;
		}
	}
	else
	{
		INFO("OpenGL: FBOs are unsupported. Some emulation features will be disabled.\n");
	}
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
	this->isMultisampledFBOSupported	= this->isFBOSupported &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_multisample");
	if (this->isMultisampledFBOSupported)
	{
		error = this->CreateMultisampledFBO();
		if (error != OGLERROR_NOERR)
		{
			this->isMultisampledFBOSupported = false;
		}
	}
	else
	{
		INFO("OpenGL: Multisampled FBOs are unsupported. Multisample antialiasing will be disabled.\n");
	}
	
	this->InitTextures();
	this->InitFinalRenderStates(&oglExtensionSet); // This must be done last
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::CreateVBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenBuffersARB(1, &OGLRef.vboGeometryVtxID);
	glGenBuffersARB(1, &OGLRef.iboGeometryIndexID);
	glGenBuffersARB(1, &OGLRef.vboPostprocessVtxID);
	glGenBuffersARB(1, &OGLRef.iboPostprocessIndexID);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboGeometryVtxID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, VERTLIST_SIZE * sizeof(VERT), NULL, GL_STREAM_DRAW_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboGeometryIndexID);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRENDER_VERT_INDEX_BUFFER_COUNT * sizeof(GLushort), NULL, GL_STREAM_DRAW_ARB);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboPostprocessVtxID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(PostprocessVtxBuffer), PostprocessVtxBuffer, GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboPostprocessIndexID);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(PostprocessElementBuffer), PostprocessElementBuffer, GL_STATIC_DRAW_ARB);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyVBOs()
{
	if (!this->isVBOSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	
	glDeleteBuffersARB(1, &OGLRef.vboGeometryVtxID);
	glDeleteBuffersARB(1, &OGLRef.iboGeometryIndexID);
	glDeleteBuffersARB(1, &OGLRef.vboPostprocessVtxID);
	glDeleteBuffersARB(1, &OGLRef.iboPostprocessIndexID);
	
	this->isVBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreatePBOs()
{
	glGenBuffersARB(2, this->ref->pboRenderDataID);
	for (size_t i = 0; i < 2; i++)
	{
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, this->ref->pboRenderDataID[i]);
		glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT * sizeof(u32), NULL, GL_STREAM_READ_ARB);
	}
	
	glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyPBOs()
{
	if (!this->isPBOSupported)
	{
		return;
	}
	
	glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	glDeleteBuffersARB(2, this->ref->pboRenderDataID);
	
	this->isPBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::LoadGeometryShaders(std::string &outVertexShaderProgram, std::string &outFragmentShaderProgram)
{
	outVertexShaderProgram.clear();
	outFragmentShaderProgram.clear();
	
	outVertexShaderProgram += std::string(vertexShader_100);
	outFragmentShaderProgram += std::string(fragmentShader_100);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitGeometryProgramBindings()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindAttribLocation(OGLRef.programGeometryID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programGeometryID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	glBindAttribLocation(OGLRef.programGeometryID, OGLVertexAttributeID_Color, "inColor");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitGeometryProgram(const std::string &vertexShaderProgram, const std::string &fragmentShaderProgram)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	OGLRef.vertexGeometryShaderID = glCreateShader(GL_VERTEX_SHADER);
	if(!OGLRef.vertexGeometryShaderID)
	{
		INFO("OpenGL: Failed to create the vertex shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");		
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *vertexShaderProgramChar = vertexShaderProgram.c_str();
	glShaderSource(OGLRef.vertexGeometryShaderID, 1, (const GLchar **)&vertexShaderProgramChar, NULL);
	glCompileShader(OGLRef.vertexGeometryShaderID);
	if (!this->ValidateShaderCompile(OGLRef.vertexGeometryShaderID))
	{
		glDeleteShader(OGLRef.vertexGeometryShaderID);
		INFO("OpenGL: Failed to compile the vertex shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.fragmentGeometryShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if(!OGLRef.fragmentGeometryShaderID)
	{
		glDeleteShader(OGLRef.vertexGeometryShaderID);
		INFO("OpenGL: Failed to create the fragment shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *fragmentShaderProgramChar = fragmentShaderProgram.c_str();
	glShaderSource(OGLRef.fragmentGeometryShaderID, 1, (const GLchar **)&fragmentShaderProgramChar, NULL);
	glCompileShader(OGLRef.fragmentGeometryShaderID);
	if (!this->ValidateShaderCompile(OGLRef.fragmentGeometryShaderID))
	{
		glDeleteShader(OGLRef.vertexGeometryShaderID);
		glDeleteShader(OGLRef.fragmentGeometryShaderID);
		INFO("OpenGL: Failed to compile the fragment shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.programGeometryID = glCreateProgram();
	if(!OGLRef.programGeometryID)
	{
		glDeleteShader(OGLRef.vertexGeometryShaderID);
		glDeleteShader(OGLRef.fragmentGeometryShaderID);
		INFO("OpenGL: Failed to create the shader program. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glAttachShader(OGLRef.programGeometryID, OGLRef.vertexGeometryShaderID);
	glAttachShader(OGLRef.programGeometryID, OGLRef.fragmentGeometryShaderID);
	
	this->InitGeometryProgramBindings();
	
	glLinkProgram(OGLRef.programGeometryID);
	if (!this->ValidateShaderProgramLink(OGLRef.programGeometryID))
	{
		glDetachShader(OGLRef.programGeometryID, OGLRef.vertexGeometryShaderID);
		glDetachShader(OGLRef.programGeometryID, OGLRef.fragmentGeometryShaderID);
		glDeleteProgram(OGLRef.programGeometryID);
		glDeleteShader(OGLRef.vertexGeometryShaderID);
		glDeleteShader(OGLRef.fragmentGeometryShaderID);
		INFO("OpenGL: Failed to link the shader program. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.programGeometryID);
	glUseProgram(OGLRef.programGeometryID);
	
	// Set up shader uniforms
	OGLRef.uniformTexRenderObject				= glGetUniformLocation(OGLRef.programGeometryID, "texRenderObject");
	OGLRef.uniformTexToonTable					= glGetUniformLocation(OGLRef.programGeometryID, "texToonTable");
	
	OGLRef.uniformStateToonShadingMode			= glGetUniformLocation(OGLRef.programGeometryID, "stateToonShadingMode");
	OGLRef.uniformStateEnableAlphaTest			= glGetUniformLocation(OGLRef.programGeometryID, "stateEnableAlphaTest");
	OGLRef.uniformStateEnableAntialiasing		= glGetUniformLocation(OGLRef.programGeometryID, "stateEnableAntialiasing");
	OGLRef.uniformStateEnableEdgeMarking		= glGetUniformLocation(OGLRef.programGeometryID, "stateEnableEdgeMarking");
	OGLRef.uniformStateEnableFogAlphaOnly		= glGetUniformLocation(OGLRef.programGeometryID, "stateEnableFogAlphaOnly");
	OGLRef.uniformStateUseWDepth				= glGetUniformLocation(OGLRef.programGeometryID, "stateUseWDepth");
	OGLRef.uniformStateAlphaTestRef				= glGetUniformLocation(OGLRef.programGeometryID, "stateAlphaTestRef");
	
	OGLRef.uniformPolyTexScale					= glGetUniformLocation(OGLRef.programGeometryID, "polyTexScale");
	OGLRef.uniformPolyMode						= glGetUniformLocation(OGLRef.programGeometryID, "polyMode");
	OGLRef.uniformPolyEnableDepthWrite			= glGetUniformLocation(OGLRef.programGeometryID, "polyEnableDepthWrite");
	OGLRef.uniformPolySetNewDepthForTranslucent	= glGetUniformLocation(OGLRef.programGeometryID, "polySetNewDepthForTranslucent");
	OGLRef.uniformPolyAlpha						= glGetUniformLocation(OGLRef.programGeometryID, "polyAlpha");
	OGLRef.uniformPolyID						= glGetUniformLocation(OGLRef.programGeometryID, "polyID");
	
	OGLRef.uniformPolyEnableTexture				= glGetUniformLocation(OGLRef.programGeometryID, "polyEnableTexture");
	OGLRef.uniformPolyEnableFog					= glGetUniformLocation(OGLRef.programGeometryID, "polyEnableFog");
	
	INFO("OpenGL: Successfully created shaders.\n");
	
	this->CreateToonTable();
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyGeometryProgram()
{
	if(!this->isShaderSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glUseProgram(0);
	
	glDetachShader(OGLRef.programGeometryID, OGLRef.vertexGeometryShaderID);
	glDetachShader(OGLRef.programGeometryID, OGLRef.fragmentGeometryShaderID);
	
	glDeleteProgram(OGLRef.programGeometryID);
	glDeleteShader(OGLRef.vertexGeometryShaderID);
	glDeleteShader(OGLRef.fragmentGeometryShaderID);
	
	this->DestroyToonTable();
	
	this->isShaderSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreateVAOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenVertexArrays(1, &OGLRef.vaoGeometryStatesID);
	glGenVertexArrays(1, &OGLRef.vaoPostprocessStatesID);
	
	glBindVertexArray(OGLRef.vaoGeometryStatesID);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboGeometryVtxID);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboGeometryIndexID);
	
	glEnableVertexAttribArray(OGLVertexAttributeID_Position);
	glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	glEnableVertexAttribArray(OGLVertexAttributeID_Color);
	glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
	glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
	glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
	
	glBindVertexArray(0);
	
	glBindVertexArray(OGLRef.vaoPostprocessStatesID);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboPostprocessVtxID);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboPostprocessIndexID);
	
	glEnableVertexAttribArray(OGLVertexAttributeID_Position);
	glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(sizeof(GLfloat) * 8));
	
	glBindVertexArray(0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyVAOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->isVAOSupported)
	{
		return;
	}
	
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &OGLRef.vaoGeometryStatesID);
	glDeleteVertexArrays(1, &OGLRef.vaoPostprocessStatesID);
	
	this->isVAOSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreateFBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up FBO render targets
	glGenTextures(1, &OGLRef.texGColorID);
	glGenTextures(1, &OGLRef.texGDepthID);
	glGenTextures(1, &OGLRef.texGPolyID);
	glGenTextures(1, &OGLRef.texGFogAttrID);
	glGenTextures(1, &OGLRef.texGDepthStencilID);
	glGenTextures(1, &OGLRef.texPostprocessEdgeMarkID);
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
	
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
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texPostprocessEdgeMarkID);
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
	glGenFramebuffersEXT(1, &OGLRef.fboRenderID);
	glGenFramebuffersEXT(1, &OGLRef.fboPostprocessID);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, OGLRef.texGColorID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, OGLRef.texGDepthID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_TEXTURE_2D, OGLRef.texGPolyID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT3_EXT, GL_TEXTURE_2D, OGLRef.texGFogAttrID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texGDepthStencilID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texGDepthStencilID, 0);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to created FBOs. Some emulation features will be disabled.\n");
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &OGLRef.fboRenderID);
		glDeleteFramebuffersEXT(1, &OGLRef.fboPostprocessID);
		glDeleteTextures(1, &OGLRef.texGColorID);
		glDeleteTextures(1, &OGLRef.texGDepthID);
		glDeleteTextures(1, &OGLRef.texGPolyID);
		glDeleteTextures(1, &OGLRef.texGFogAttrID);
		glDeleteTextures(1, &OGLRef.texGDepthStencilID);
		glDeleteTextures(1, &OGLRef.texPostprocessEdgeMarkID);
		glDeleteTextures(1, &OGLRef.texPostprocessFogID);
		
		this->isFBOSupported = false;
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glDrawBuffers(4, RenderDrawList);
	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboPostprocessID);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, OGLRef.texPostprocessEdgeMarkID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, OGLRef.texPostprocessFogID, 0);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to created FBOs. Some emulation features will be disabled.\n");
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &OGLRef.fboRenderID);
		glDeleteFramebuffersEXT(1, &OGLRef.fboPostprocessID);
		glDeleteTextures(1, &OGLRef.texGColorID);
		glDeleteTextures(1, &OGLRef.texGDepthID);
		glDeleteTextures(1, &OGLRef.texGPolyID);
		glDeleteTextures(1, &OGLRef.texGFogAttrID);
		glDeleteTextures(1, &OGLRef.texGDepthStencilID);
		glDeleteTextures(1, &OGLRef.texPostprocessEdgeMarkID);
		glDeleteTextures(1, &OGLRef.texPostprocessFogID);
		
		this->isFBOSupported = false;
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	
	OGLRef.selectedRenderingFBO = OGLRef.fboRenderID;
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
	INFO("OpenGL: Successfully created FBOs.\n");
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyFBOs()
{
	if (!this->isFBOSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glDeleteFramebuffersEXT(1, &OGLRef.fboRenderID);
	glDeleteFramebuffersEXT(1, &OGLRef.fboPostprocessID);
	glDeleteTextures(1, &OGLRef.texGColorID);
	glDeleteTextures(1, &OGLRef.texGDepthID);
	glDeleteTextures(1, &OGLRef.texGPolyID);
	glDeleteTextures(1, &OGLRef.texGFogAttrID);
	glDeleteTextures(1, &OGLRef.texGDepthStencilID);
	glDeleteTextures(1, &OGLRef.texPostprocessEdgeMarkID);
	
	OGLRef.fboRenderID = 0;
	OGLRef.fboPostprocessID = 0;
	
	this->isFBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreateMultisampledFBO()
{
	// Check the maximum number of samples that the driver supports and use that.
	// Since our target resolution is only 256x192 pixels, using the most samples
	// possible is the best thing to do.
	GLint maxSamples = 0;
	glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSamples);
	
	if (maxSamples < 2)
	{
		INFO("OpenGL: Driver does not support at least 2x multisampled FBOs. Multisample antialiasing will be disabled.\n");
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	else if (maxSamples > OGLRENDER_MAX_MULTISAMPLES)
	{
		maxSamples = OGLRENDER_MAX_MULTISAMPLES;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up FBO render targets
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGColorID);
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGDepthID);
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGPolyID);
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGFogAttrID);
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGDepthStencilID);
	
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGColorID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, maxSamples, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, maxSamples, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGPolyID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, maxSamples, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGFogAttrID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, maxSamples, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, maxSamples, GL_DEPTH24_STENCIL8_EXT, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	
	// Set up multisampled rendering FBO
	glGenFramebuffersEXT(1, &OGLRef.fboMSIntermediateRenderID);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboMSIntermediateRenderID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGColorID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGPolyID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT3_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGFogAttrID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilID);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to create multisampled FBO. Multisample antialiasing will be disabled.\n");
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &OGLRef.fboMSIntermediateRenderID);
		glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGColorID);
		glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGDepthID);
		glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGPolyID);
		glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGFogAttrID);
		glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGDepthStencilID);
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
	INFO("OpenGL: Successfully created multisampled FBO.\n");
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyMultisampledFBO()
{
	if (!this->isMultisampledFBOSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glDeleteFramebuffersEXT(1, &OGLRef.fboMSIntermediateRenderID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGColorID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGDepthID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGPolyID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGFogAttrID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGDepthStencilID);
	
	this->isMultisampledFBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::InitFinalRenderStates(const std::set<std::string> *oglExtensionSet)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	bool isTexMirroredRepeatSupported = this->IsExtensionPresent(oglExtensionSet, "GL_ARB_texture_mirrored_repeat");
	bool isBlendFuncSeparateSupported = this->IsExtensionPresent(oglExtensionSet, "GL_EXT_blend_func_separate");
	bool isBlendEquationSeparateSupported = this->IsExtensionPresent(oglExtensionSet, "GL_EXT_blend_equation_separate");
	
	// Blending Support
	if (isBlendFuncSeparateSupported)
	{
		if (isBlendEquationSeparateSupported)
		{
			// we want to use alpha destination blending so we can track the last-rendered alpha value
			// test: new super mario brothers renders the stormclouds at the beginning
			glBlendFuncSeparateEXT(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_DST_ALPHA);
			glBlendEquationSeparateEXT(GL_FUNC_ADD, GL_MAX);
		}
		else
		{
			glBlendFuncSeparateEXT(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_DST_ALPHA);
		}
	}
	else
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	
	// Mirrored Repeat Mode Support
	OGLRef.stateTexMirroredRepeat = isTexMirroredRepeatSupported ? GL_MIRRORED_REPEAT : GL_REPEAT;
	
	// Always enable depth test, and control using glDepthMask().
	glEnable(GL_DEPTH_TEST);
	
	// Map the vertex list's colors with 4 floats per color. This is being done
	// because OpenGL needs 4-colors per vertex to support translucency. (The DS
	// uses 3-colors per vertex, and adds alpha through the poly, so we can't
	// simply reference the colors+alpha from just the vertices by themselves.)
	OGLRef.color4fBuffer = this->isShaderSupported ? NULL : new GLfloat[VERTLIST_SIZE * 4];
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitTextures()
{
	this->ExpandFreeTextures();
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitTables()
{
	static bool needTableInit = true;
	
	if (needTableInit)
	{
		for (size_t i = 0; i < 256; i++)
			material_8bit_to_float[i] = (GLfloat)(i * 4) / 255.0f;
		
		needTableInit = false;
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitEdgeMarkProgramBindings()
{
	return OGLERROR_FEATURE_UNSUPPORTED;
}

Render3DError OpenGLRenderer_1_2::InitPostprocessingPrograms(const std::string &edgeMarkVtxShader, const std::string &edgeMarkFragShader, const std::string &fogVtxShader, const std::string &fogFragShader)
{
	return OGLERROR_FEATURE_UNSUPPORTED;
}

Render3DError OpenGLRenderer_1_2::InitFogProgramBindings()
{
	return OGLERROR_FEATURE_UNSUPPORTED;
}

Render3DError OpenGLRenderer_1_2::DestroyPostprocessingPrograms()
{
	return OGLERROR_FEATURE_UNSUPPORTED;
}

Render3DError OpenGLRenderer_1_2::CreateToonTable()
{
	OGLRenderRef &OGLRef = *this->ref;
	u16 tempToonTable[32];
	memset(tempToonTable, 0, sizeof(tempToonTable));
	
	// The toon table is a special 1D texture where each pixel corresponds
	// to a specific color in the toon table.
	glGenTextures(1, &OGLRef.texToonTableID);
	glBindTexture(GL_TEXTURE_1D, OGLRef.texToonTableID);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, tempToonTable);
	glBindTexture(GL_TEXTURE_1D, 0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DestroyToonTable()
{
	glDeleteTextures(1, &this->ref->texToonTableID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::UploadToonTable(const u16 *toonTableBuffer)
{
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ToonTable);
	glBindTexture(GL_TEXTURE_1D, this->ref->texToonTableID);
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 32, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, toonTableBuffer);
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::UploadClearImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthStencilBuffer, const bool *__restrict fogBuffer)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	static GLuint depth[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT];
	static GLuint polyID[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT];
	static GLuint fogAttributes[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT];
	
	for (size_t i = 0; i < GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT; i++)
	{
		depth[i] = clearImageDepthStencilBuffer[i] >> 8;
		polyID[i] = (depthStencilBuffer[i] & 0x0000003F) | 0xFF000000;
		fogAttributes[i] = (fogBuffer[i]) ? 0xFF0000FF : 0xFF000000;
	}
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_GColor);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthStencilID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, depthStencilBuffer);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGColorID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, colorBuffer);
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_GDepth);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, depth);
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_GPolyID);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGPolyID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, polyID);
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_FogAttr);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGFogAttrID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, fogAttributes);
	
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::GetExtensionSet(std::set<std::string> *oglExtensionSet)
{
	std::string oglExtensionString = std::string((const char *)glGetString(GL_EXTENSIONS));
	
	size_t extStringStartLoc = 0;
	size_t delimiterLoc = oglExtensionString.find_first_of(' ', extStringStartLoc);
	while (delimiterLoc != std::string::npos)
	{
		std::string extensionName = oglExtensionString.substr(extStringStartLoc, delimiterLoc - extStringStartLoc);
		oglExtensionSet->insert(extensionName);
		
		extStringStartLoc = delimiterLoc + 1;
		delimiterLoc = oglExtensionString.find_first_of(' ', extStringStartLoc);
	}
	
	if (extStringStartLoc - oglExtensionString.length() > 0)
	{
		std::string extensionName = oglExtensionString.substr(extStringStartLoc, oglExtensionString.length() - extStringStartLoc);
		oglExtensionSet->insert(extensionName);
	}
}

Render3DError OpenGLRenderer_1_2::ExpandFreeTextures()
{
	static const GLsizei kInitTextures = 128;
	GLuint oglTempTextureID[kInitTextures];
	glGenTextures(kInitTextures, oglTempTextureID);
	
	for(GLsizei i = 0; i < kInitTextures; i++)
	{
		this->ref->freeTextureIDs.push(oglTempTextureID[i]);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupVertices(const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList, GLushort *outIndexBuffer, size_t *outIndexCount)
{
	OGLRenderRef &OGLRef = *this->ref;
	const size_t polyCount = polyList->count;
	size_t vertIndexCount = 0;
	
	for(size_t i = 0; i < polyCount; i++)
	{
		const POLY *poly = &polyList->list[indexList->list[i]];
		const size_t polyType = poly->type;
		
		if (this->isShaderSupported)
		{
			for(size_t j = 0; j < polyType; j++)
			{
				const GLushort vertIndex = poly->vertIndexes[j];
				
				// While we're looping through our vertices, add each vertex index to
				// a buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
				// vertices here to convert them to GL_TRIANGLES, which are much easier
				// to work with and won't be deprecated in future OpenGL versions.
				outIndexBuffer[vertIndexCount++] = vertIndex;
				if (poly->vtxFormat == GFX3D_QUADS || poly->vtxFormat == GFX3D_QUAD_STRIP)
				{
					if (j == 2)
					{
						outIndexBuffer[vertIndexCount++] = vertIndex;
					}
					else if (j == 3)
					{
						outIndexBuffer[vertIndexCount++] = poly->vertIndexes[0];
					}
				}
			}
		}
		else
		{
			const GLfloat thePolyAlpha = (!poly->isWireframe() && poly->isTranslucent()) ? divide5bitBy31_LUT[poly->getAttributeAlpha()] : 1.0f;
			
			for(size_t j = 0; j < polyType; j++)
			{
				const GLushort vertIndex = poly->vertIndexes[j];
				const size_t colorIndex = vertIndex * 4;
				
				// Consolidate the vertex color and the poly alpha to our internal color buffer
				// so that OpenGL can use it.
				const VERT *vert = &vertList->list[vertIndex];
				OGLRef.color4fBuffer[colorIndex+0] = material_8bit_to_float[vert->color[0]];
				OGLRef.color4fBuffer[colorIndex+1] = material_8bit_to_float[vert->color[1]];
				OGLRef.color4fBuffer[colorIndex+2] = material_8bit_to_float[vert->color[2]];
				OGLRef.color4fBuffer[colorIndex+3] = thePolyAlpha;
				
				// While we're looping through our vertices, add each vertex index to a
				// buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
				// vertices here to convert them to GL_TRIANGLES, which are much easier
				// to work with and won't be deprecated in future OpenGL versions.
				outIndexBuffer[vertIndexCount++] = vertIndex;
				if (poly->vtxFormat == GFX3D_QUADS || poly->vtxFormat == GFX3D_QUAD_STRIP)
				{
					if (j == 2)
					{
						outIndexBuffer[vertIndexCount++] = vertIndex;
					}
					else if (j == 3)
					{
						outIndexBuffer[vertIndexCount++] = poly->vertIndexes[0];
					}
				}
			}
		}
	}
	
	*outIndexCount = vertIndexCount;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const size_t vertIndexCount)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(OGLRef.vaoGeometryStatesID);
		glBindBuffer(GL_ARRAY_BUFFER_ARB, OGLRef.vboGeometryVtxID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboGeometryIndexID);
		
		glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VERT) * vertList->count, vertList);
		glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
	}
	else
	{
		if (this->isShaderSupported)
		{
			glEnableVertexAttribArray(OGLVertexAttributeID_Position);
			glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
			glEnableVertexAttribArray(OGLVertexAttributeID_Color);
			
			if (this->isVBOSupported)
			{
				glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboGeometryIndexID);
				glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0, vertIndexCount * sizeof(GLushort), OGLRef.vertIndexBuffer);
				
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboGeometryVtxID);
				glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VERT) * vertList->count, vertList);
				glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
				glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
				glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
			}
			else
			{
				glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), &vertList->list[0].coord);
				glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), &vertList->list[0].texcoord);
				glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), &vertList->list[0].color);
			}
		}
		else
		{
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glEnableClientState(GL_VERTEX_ARRAY);
			
			if (this->isVBOSupported)
			{
				glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboGeometryIndexID);
				glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0, vertIndexCount * sizeof(GLushort), OGLRef.vertIndexBuffer);
				
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
				glColorPointer(4, GL_FLOAT, 0, OGLRef.color4fBuffer);
				
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboGeometryVtxID);
				glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VERT) * vertList->count, vertList);
				glVertexPointer(4, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
				glTexCoordPointer(2, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
			}
			else
			{
				glVertexPointer(4, GL_FLOAT, sizeof(VERT), &vertList->list[0].coord);
				glTexCoordPointer(2, GL_FLOAT, sizeof(VERT), &vertList->list[0].texcoord);
				glColorPointer(4, GL_FLOAT, 0, OGLRef.color4fBuffer);
			}
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DisableVertexAttributes()
{
	if (this->isVAOSupported)
	{
		glBindVertexArray(0);
	}
	else
	{
		if (this->isShaderSupported)
		{
			glDisableVertexAttribArray(OGLVertexAttributeID_Position);
			glDisableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
			glDisableVertexAttribArray(OGLVertexAttributeID_Color);
		}
		else
		{
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		
		if (this->isVBOSupported)
		{
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SelectRenderingFramebuffer()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isMultisampledFBOSupported)
	{
		OGLRef.selectedRenderingFBO = (CommonSettings.GFX3D_Renderer_Multisample) ? OGLRef.fboMSIntermediateRenderID : OGLRef.fboRenderID;
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DownsampleFBO()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isMultisampledFBOSupported && OGLRef.selectedRenderingFBO == OGLRef.fboMSIntermediateRenderID)
	{
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboMSIntermediateRenderID);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
		
		// Blit the color buffer
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glBlitFramebufferEXT(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the working depth buffer
		glReadBuffer(GL_COLOR_ATTACHMENT1_EXT);
		glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);
		glBlitFramebufferEXT(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the polygon ID buffer
		glReadBuffer(GL_COLOR_ATTACHMENT2_EXT);
		glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
		glBlitFramebufferEXT(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the fog buffer
		glReadBuffer(GL_COLOR_ATTACHMENT3_EXT);
		glDrawBuffer(GL_COLOR_ATTACHMENT3_EXT);
		glBlitFramebufferEXT(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Reset framebuffer targets
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glDrawBuffers(4, RenderDrawList);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ReadBackPixels()
{
	const size_t i = this->doubleBufferIndex;
	
	if (this->isPBOSupported)
	{
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, this->ref->pboRenderDataID[i]);
		glReadPixels(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, 0);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	
	this->gpuScreen3DHasNewData[i] = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DeleteTexture(const TexCacheItem *item)
{
	this->ref->freeTextureIDs.push((GLuint)item->texid);
	if(this->currTexture == item)
	{
		this->currTexture = NULL;
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::BeginRender(const GFX3D_State *renderState)
{
	OGLRenderRef &OGLRef = *this->ref;
	this->doubleBufferIndex = (this->doubleBufferIndex + 1) & 0x01;
	
	this->SelectRenderingFramebuffer();
	
	if (this->isShaderSupported)
	{
		glUseProgram(OGLRef.programGeometryID);
		glUniform1i(OGLRef.uniformStateToonShadingMode, renderState->shading);
		glUniform1i(OGLRef.uniformStateEnableAlphaTest, (renderState->enableAlphaTest) ? GL_TRUE : GL_FALSE);
		glUniform1i(OGLRef.uniformStateEnableAntialiasing, (renderState->enableAntialiasing) ? GL_TRUE : GL_FALSE);
		glUniform1i(OGLRef.uniformStateEnableEdgeMarking, (renderState->enableEdgeMarking) ? GL_TRUE : GL_FALSE);
		glUniform1i(OGLRef.uniformStateEnableFogAlphaOnly, (renderState->enableFogAlphaOnly) ? GL_TRUE : GL_FALSE);
		glUniform1i(OGLRef.uniformStateUseWDepth, (renderState->wbuffer) ? GL_TRUE : GL_FALSE);
		glUniform1f(OGLRef.uniformStateAlphaTestRef, divide5bitBy31_LUT[renderState->alphaTestRef]);
		glUniform1i(OGLRef.uniformTexRenderObject, 0);
		glUniform1i(OGLRef.uniformTexToonTable, OGLTextureUnitID_ToonTable);
		
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_STENCIL_TEST);
	}
	else
	{
		if(renderState->enableAlphaTest && (renderState->alphaTestRef > 0))
		{
			glAlphaFunc(GL_GEQUAL, divide5bitBy31_LUT[renderState->alphaTestRef]);
		}
		else
		{
			glAlphaFunc(GL_GREATER, 0);
		}
		
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
	}
	
	if(renderState->enableAlphaBlending)
	{
		glEnable(GL_BLEND);
	}
	else
	{
		glDisable(GL_BLEND);
	}
	
	glDepthMask(GL_TRUE);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::RenderGeometry(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList)
{
	OGLRenderRef &OGLRef = *this->ref;
	const size_t polyCount = polyList->count;
	
	// Map GFX3D_QUADS and GFX3D_QUAD_STRIP to GL_TRIANGLES since we will convert them.
	//
	// Also map GFX3D_TRIANGLE_STRIP to GL_TRIANGLES. This is okay since this is actually
	// how the POLY struct stores triangle strip vertices, which is in sets of 3 vertices
	// each. This redefinition is necessary since uploading more than 3 indices at a time
	// will cause glDrawElements() to draw the triangle strip incorrectly.
	static const GLenum oglPrimitiveType[]	= {GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES,
											   GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_STRIP, GL_LINE_STRIP};
	
	static const GLsizei indexIncrementLUT[] = {3, 6, 3, 6, 3, 4, 3, 4};
	
	if (polyCount > 0)
	{
		size_t vertIndexBufferCount = 0;
		this->SetupVertices(vertList, polyList, indexList, OGLRef.vertIndexBuffer, &vertIndexBufferCount);
		this->EnableVertexAttributes(vertList, OGLRef.vertIndexBuffer, vertIndexBufferCount);
		
		const POLY *firstPoly = &polyList->list[indexList->list[0]];
		u32 lastPolyAttr = firstPoly->polyAttr;
		u32 lastTexParams = firstPoly->texParam;
		u32 lastTexPalette = firstPoly->texPalette;
		u32 lastViewport = firstPoly->viewport;
		
		this->SetupPolygon(firstPoly);
		this->SetupTexture(firstPoly, renderState->enableTexturing);
		this->SetupViewport(lastViewport);
		
		GLsizei vertIndexCount = 0;
		GLushort *indexBufferPtr = (this->isVBOSupported) ? 0 : OGLRef.vertIndexBuffer;
		
		// Enumerate through all polygons and render
		for(size_t i = 0; i < polyCount; i++)
		{
			const POLY *poly = &polyList->list[indexList->list[i]];
			
			// Set up the polygon if it changed
			if(lastPolyAttr != poly->polyAttr)
			{
				lastPolyAttr = poly->polyAttr;
				this->SetupPolygon(poly);
			}
			
			// Set up the texture if it changed
			if(lastTexParams != poly->texParam || lastTexPalette != poly->texPalette)
			{
				lastTexParams = poly->texParam;
				lastTexPalette = poly->texPalette;
				this->SetupTexture(poly, renderState->enableTexturing);
			}
			
			// Set up the viewport if it changed
			if(lastViewport != poly->viewport)
			{
				lastViewport = poly->viewport;
				this->SetupViewport(poly->viewport);
			}
			
			// In wireframe mode, redefine all primitives as GL_LINE_LOOP rather than
			// setting the polygon mode to GL_LINE though glPolygonMode(). Not only is
			// drawing more accurate this way, but it also allows GFX3D_QUADS and
			// GFX3D_QUAD_STRIP primitives to properly draw as wireframe without the
			// extra diagonal line.
			const GLenum polyPrimitive = (!poly->isWireframe()) ? oglPrimitiveType[poly->vtxFormat] : GL_LINE_LOOP;
			
			// Increment the vertex count
			vertIndexCount += indexIncrementLUT[poly->vtxFormat];
			
			// Look ahead to the next polygon to see if we can simply buffer the indices
			// instead of uploading them now. We can buffer if all polygon states remain
			// the same and we're not drawing a line loop or line strip.
			if (i+1 < polyCount)
			{
				const POLY *nextPoly = &polyList->list[indexList->list[i+1]];
				
				if (lastPolyAttr == nextPoly->polyAttr &&
					lastTexParams == nextPoly->texParam &&
					lastTexPalette == nextPoly->texPalette &&
					lastViewport == nextPoly->viewport &&
					polyPrimitive == oglPrimitiveType[nextPoly->vtxFormat] &&
					polyPrimitive != GL_LINE_LOOP &&
					polyPrimitive != GL_LINE_STRIP &&
					oglPrimitiveType[nextPoly->vtxFormat] != GL_LINE_LOOP &&
					oglPrimitiveType[nextPoly->vtxFormat] != GL_LINE_STRIP)
				{
					continue;
				}
			}
			
			// Render the polygons
			glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
			indexBufferPtr += vertIndexCount;
			vertIndexCount = 0;
		}
		
		this->DisableVertexAttributes();
	}
	
	this->DownsampleFBO();
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::EndRender(const u64 frameCount)
{
	//needs to happen before endgl because it could free some textureids for expired cache items
	TexCache_EvictFrame();
	
	this->ReadBackPixels();
	this->didRenderEdgeMark = false;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::UpdateToonTable(const u16 *toonTableBuffer)
{
	this->UploadToonTable(toonTableBuffer);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthStencilBuffer, const bool *__restrict fogBuffer)
{
	if (!this->isFBOSupported)
	{
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	this->UploadClearImage(colorBuffer, depthStencilBuffer, fogBuffer);
	
	if (this->isMultisampledFBOSupported && OGLRef.selectedRenderingFBO == OGLRef.fboMSIntermediateRenderID)
	{
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.fboMSIntermediateRenderID);
		
		// Blit the color buffer
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glBlitFramebufferEXT(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		
		// It might seem wasteful to be doing a separate glClear(GL_STENCIL_BUFFER_BIT) instead
		// of simply blitting the stencil buffer with everything else.
		//
		// We do this because glBlitFramebufferEXT() for GL_STENCIL_BUFFER_BIT has been tested
		// to be unsupported on ATI/AMD GPUs running in compatibility mode. So we do the separate
		// glClear() for GL_STENCIL_BUFFER_BIT to keep these GPUs working.
		glClearStencil(depthStencilBuffer[0] & 0x3F);
		glClear(GL_STENCIL_BUFFER_BIT);
		
		// Blit the working depth buffer
		glReadBuffer(GL_COLOR_ATTACHMENT1_EXT);
		glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);
		glBlitFramebufferEXT(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the polygon ID buffer
		glReadBuffer(GL_COLOR_ATTACHMENT2_EXT);
		glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
		glBlitFramebufferEXT(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the fog buffer
		glReadBuffer(GL_COLOR_ATTACHMENT3_EXT);
		glDrawBuffer(GL_COLOR_ATTACHMENT3_EXT);
		glBlitFramebufferEXT(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Reset framebuffer targets
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glDrawBuffers(4, RenderDrawList);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboMSIntermediateRenderID);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ClearUsingValues(const u8 r, const u8 g, const u8 b, const u8 a, const u32 clearDepth, const u8 clearPolyID, const bool enableFog) const
{
	if (this->isShaderSupported && this->isFBOSupported)
	{
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); // texGColorID
		glClearColor(divide5bitBy31_LUT[r], divide5bitBy31_LUT[g], divide5bitBy31_LUT[b], divide5bitBy31_LUT[a]);
		glClearDepth((GLclampd)clearDepth / (GLclampd)0x00FFFFFF);
		glClearStencil(clearPolyID);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		
		glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT); // texGDepthID
		glClearColor((GLfloat)(clearDepth & 0x000000FF)/255.0f, (GLfloat)((clearDepth >> 8) & 0x000000FF)/255.0f, (GLfloat)((clearDepth >> 16) & 0x000000FF)/255.0f, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		
		glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT); // texGPolyID
		glClearColor((GLfloat)clearPolyID/63.0f, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		
		glDrawBuffer(GL_COLOR_ATTACHMENT3_EXT); // texGFogAttrID
		glClearColor((enableFog) ? 1.0 : 0.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		
		glDrawBuffers(4, RenderDrawList);
	}
	else
	{
		glClearColor(divide5bitBy31_LUT[r], divide5bitBy31_LUT[g], divide5bitBy31_LUT[b], divide5bitBy31_LUT[a]);
		glClearDepth((GLclampd)clearDepth / (GLclampd)0x00FFFFFF);
		glClearStencil(clearPolyID);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupPolygon(const POLY *thePoly)
{
	OGLRenderRef &OGLRef = *this->ref;
	const PolygonAttributes attr = thePoly->getAttributes();
	
	// Set up polygon attributes
	if (this->isShaderSupported)
	{
		glUniform1i(OGLRef.uniformPolyMode, attr.polygonMode);
		glUniform1i(OGLRef.uniformPolyEnableFog, (attr.enableRenderFog) ? GL_TRUE : GL_FALSE);
		glUniform1f(OGLRef.uniformPolyAlpha, (!attr.isWireframe && attr.isTranslucent) ? divide5bitBy31_LUT[attr.alpha] : 1.0f);
		glUniform1i(OGLRef.uniformPolyID, attr.polygonID);
	}
	else
	{
		// Set the texture blending mode
		static const GLint oglTexBlendMode[4] = {GL_MODULATE, GL_DECAL, GL_MODULATE, GL_MODULATE};
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, oglTexBlendMode[attr.polygonMode]);
	}
	
	// Set up depth test mode
	static const GLenum oglDepthFunc[2] = {GL_LESS, GL_EQUAL};
	glDepthFunc(oglDepthFunc[attr.enableDepthTest]);
	
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
	if(attr.polygonMode == 3)
	{
		glEnable(GL_STENCIL_TEST);
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
		glEnable(GL_STENCIL_TEST);
		if(attr.isTranslucent)
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
	
	if(attr.isTranslucent && !attr.enableAlphaDepthWrite)
	{
		enableDepthWrite = GL_FALSE;
	}
	
	glDepthMask(enableDepthWrite);
	
	if (this->isShaderSupported)
	{
		glUniform1i(OGLRef.uniformPolyEnableDepthWrite, enableDepthWrite);
		glUniform1i(OGLRef.uniformPolySetNewDepthForTranslucent, (attr.enableAlphaDepthWrite) ? GL_TRUE : GL_FALSE);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupTexture(const POLY *thePoly, bool enableTexturing)
{
	OGLRenderRef &OGLRef = *this->ref;
	const PolygonTexParams params = thePoly->getTexParams();
	
	// Check if we need to use textures
	if (thePoly->texParam == 0 || params.texFormat == TEXMODE_NONE || !enableTexturing)
	{
		if (this->isShaderSupported)
		{
			glUniform1i(OGLRef.uniformPolyEnableTexture, GL_FALSE);
		}
		else
		{
			glDisable(GL_TEXTURE_2D);
		}
		
		return OGLERROR_NOERR;
	}
	
	// Enable textures if they weren't already enabled
	if (this->isShaderSupported)
	{
		glUniform1i(OGLRef.uniformPolyEnableTexture, GL_TRUE);
	}
	else
	{
		glEnable(GL_TEXTURE_2D);
	}
	
	//	texCacheUnit.TexCache_SetTexture<TexFormat_32bpp>(format, texpal);
	TexCacheItem *newTexture = TexCache_SetTexture(TexFormat_32bpp, thePoly->texParam, thePoly->texPalette);
	if(newTexture != this->currTexture)
	{
		this->currTexture = newTexture;
		//has the ogl renderer initialized the texture?
		if(!this->currTexture->deleteCallback)
		{
			this->currTexture->deleteCallback = texDeleteCallback;
			
			if(OGLRef.freeTextureIDs.empty())
			{
				this->ExpandFreeTextures();
			}
			
			this->currTexture->texid = (u64)OGLRef.freeTextureIDs.front();
			OGLRef.freeTextureIDs.pop();
			
			glBindTexture(GL_TEXTURE_2D, (GLuint)this->currTexture->texid);
			
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (params.enableRepeatS ? (params.enableMirroredRepeatS ? OGLRef.stateTexMirroredRepeat : GL_REPEAT) : GL_CLAMP_TO_EDGE));
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (params.enableRepeatT ? (params.enableMirroredRepeatT ? OGLRef.stateTexMirroredRepeat : GL_REPEAT) : GL_CLAMP_TO_EDGE));
			
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
						 this->currTexture->sizeX, this->currTexture->sizeY, 0,
						 GL_RGBA, GL_UNSIGNED_BYTE, this->currTexture->decoded);
		}
		else
		{
			//otherwise, just bind it
			glBindTexture(GL_TEXTURE_2D, (GLuint)this->currTexture->texid);
		}
		
		if (this->isShaderSupported)
		{
			glUniform2f(OGLRef.uniformPolyTexScale, this->currTexture->invSizeX, this->currTexture->invSizeY);
		}
		else
		{
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();
			glScalef(this->currTexture->invSizeX, this->currTexture->invSizeY, 1.0f);
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupViewport(const u32 viewportValue)
{
	VIEWPORT viewport;
	viewport.decode(viewportValue);
	glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::Reset()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	this->gpuScreen3DHasNewData[0] = false;
	this->gpuScreen3DHasNewData[1] = false;
	didRenderEdgeMark = false;
	
	glFinish();
	
	for (size_t i = 0; i < 2; i++)
	{
		memset(this->GPU_screen3D[i], 0, sizeof(this->GPU_screen3D[i]));
	}
	
	if(this->isShaderSupported)
	{
		glUseProgram(OGLRef.programGeometryID);
		glUniform1i(OGLRef.uniformStateToonShadingMode, 0);
		glUniform1i(OGLRef.uniformStateEnableAlphaTest, GL_TRUE);
		glUniform1i(OGLRef.uniformStateEnableAntialiasing, GL_FALSE);
		glUniform1i(OGLRef.uniformStateEnableEdgeMarking, GL_TRUE);
		glUniform1i(OGLRef.uniformStateEnableFogAlphaOnly, GL_FALSE);
		glUniform1i(OGLRef.uniformStateUseWDepth, GL_FALSE);
		glUniform1f(OGLRef.uniformStateAlphaTestRef, 0.0f);
		
		glUniform2f(OGLRef.uniformPolyTexScale, 1.0f, 1.0f);
		glUniform1i(OGLRef.uniformPolyMode, 0);
		glUniform1i(OGLRef.uniformPolyEnableDepthWrite, GL_TRUE);
		glUniform1i(OGLRef.uniformPolySetNewDepthForTranslucent, GL_TRUE);
		glUniform1f(OGLRef.uniformPolyAlpha, 1.0f);
		glUniform1i(OGLRef.uniformPolyID, 0);
		
		glUniform1i(OGLRef.uniformPolyEnableTexture, GL_TRUE);
		glUniform1i(OGLRef.uniformPolyEnableFog, GL_FALSE);
	}
	else
	{
		glEnable(GL_NORMALIZE);
		glEnable(GL_TEXTURE_1D);
		glEnable(GL_TEXTURE_2D);
		glAlphaFunc(GL_GREATER, 0);
		glEnable(GL_ALPHA_TEST);
		glEnable(GL_BLEND);
		
		memset(OGLRef.color4fBuffer, 0, VERTLIST_SIZE * 4 * sizeof(GLfloat));
	}
	
	memset(OGLRef.vertIndexBuffer, 0, OGLRENDER_VERT_INDEX_BUFFER_COUNT * sizeof(GLushort));
	this->currTexture = NULL;
	this->doubleBufferIndex = 0;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::RenderFinish()
{
	const size_t i = this->doubleBufferIndex;
	
	if (!this->gpuScreen3DHasNewData[i])
	{
		return OGLERROR_NOERR;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isPBOSupported)
	{
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, OGLRef.pboRenderDataID[i]);
		
		const u32 *__restrict mappedBufferPtr = (u32 *__restrict)glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
		if (mappedBufferPtr != NULL)
		{
			this->ConvertFramebuffer(mappedBufferPtr, (u32 *)gfx3d_convertedScreen);
			glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
		}
		
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	else
	{
		u32 *__restrict workingBuffer = this->GPU_screen3D[i];
		glReadPixels(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, workingBuffer);
		this->ConvertFramebuffer(workingBuffer, (u32 *)gfx3d_convertedScreen);
	}
	
	this->gpuScreen3DHasNewData[i] = false;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_3::UploadToonTable(const u16 *toonTableBuffer)
{
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ToonTable);
	glBindTexture(GL_TEXTURE_1D, this->ref->texToonTableID);
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 32, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, toonTableBuffer);
	glActiveTexture(GL_TEXTURE0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_3::UploadClearImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthStencilBuffer, const bool *__restrict fogBuffer)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	static GLuint depth[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT];
	static GLuint polyID[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT];
	static GLuint fogAttributes[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT];
	
	for (size_t i = 0; i < GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT; i++)
	{
		depth[i] = (depthStencilBuffer[i] >> 8) | 0xFF000000;
		polyID[i] = (depthStencilBuffer[i] & 0x0000003F) | 0xFF000000;
		fogAttributes[i] = (fogBuffer[i]) ? 0xFF0000FF : 0xFF000000;
	}

	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GColor);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthStencilID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, depthStencilBuffer);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGColorID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, colorBuffer);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GDepth);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, depth);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GPolyID);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGPolyID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, polyID);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FogAttr);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGFogAttrID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, fogAttributes);
	
	glActiveTexture(GL_TEXTURE0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_4::InitFinalRenderStates(const std::set<std::string> *oglExtensionSet)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	bool isBlendEquationSeparateSupported = this->IsExtensionPresent(oglExtensionSet, "GL_EXT_blend_equation_separate");
	
	// Blending Support
	if (isBlendEquationSeparateSupported)
	{
		// we want to use alpha destination blending so we can track the last-rendered alpha value
		// test: new super mario brothers renders the stormclouds at the beginning
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_DST_ALPHA);
		glBlendEquationSeparateEXT(GL_FUNC_ADD, GL_MAX);
	}
	else
	{
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_DST_ALPHA);
	}
	
	// Mirrored Repeat Mode Support
	OGLRef.stateTexMirroredRepeat = GL_MIRRORED_REPEAT;
	
	// Always enable depth test, and control using glDepthMask().
	glEnable(GL_DEPTH_TEST);
	
	// Map the vertex list's colors with 4 floats per color. This is being done
	// because OpenGL needs 4-colors per vertex to support translucency. (The DS
	// uses 3-colors per vertex, and adds alpha through the poly, so we can't
	// simply reference the colors+alpha from just the vertices by themselves.)
	OGLRef.color4fBuffer = this->isShaderSupported ? NULL : new GLfloat[VERTLIST_SIZE * 4];
	
	return OGLERROR_NOERR;
}

OpenGLRenderer_1_5::~OpenGLRenderer_1_5()
{
	glFinish();
	
	DestroyVAOs();
	DestroyVBOs();
	DestroyPBOs();
}

Render3DError OpenGLRenderer_1_5::CreateVBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenBuffers(1, &OGLRef.vboGeometryVtxID);
	glGenBuffers(1, &OGLRef.iboGeometryIndexID);
	glGenBuffers(1, &OGLRef.vboPostprocessVtxID);
	glGenBuffers(1, &OGLRef.iboPostprocessIndexID);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
	glBufferData(GL_ARRAY_BUFFER, VERTLIST_SIZE * sizeof(VERT), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, OGLRENDER_VERT_INDEX_BUFFER_COUNT * sizeof(GLushort), NULL, GL_STREAM_DRAW);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(PostprocessVtxBuffer), PostprocessVtxBuffer, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboPostprocessIndexID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(PostprocessElementBuffer), PostprocessElementBuffer, GL_STATIC_DRAW);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_5::DestroyVBOs()
{
	if (!this->isVBOSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	glDeleteBuffers(1, &OGLRef.vboGeometryVtxID);
	glDeleteBuffers(1, &OGLRef.iboGeometryIndexID);
	glDeleteBuffers(1, &OGLRef.vboPostprocessVtxID);
	glDeleteBuffers(1, &OGLRef.iboPostprocessIndexID);
	
	this->isVBOSupported = false;
}

Render3DError OpenGLRenderer_1_5::CreatePBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenBuffers(2, OGLRef.pboRenderDataID);
	for (size_t i = 0; i < 2; i++)
	{
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, OGLRef.pboRenderDataID[i]);
		glBufferData(GL_PIXEL_PACK_BUFFER_ARB, GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT * sizeof(u32), NULL, GL_STREAM_READ);
	}
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_5::DestroyPBOs()
{
	if (!this->isPBOSupported)
	{
		return;
	}
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
	glDeleteBuffers(2, this->ref->pboRenderDataID);
	
	this->isPBOSupported = false;
}

Render3DError OpenGLRenderer_1_5::CreateVAOs()
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

Render3DError OpenGLRenderer_1_5::EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const size_t vertIndexCount)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(OGLRef.vaoGeometryStatesID);
		glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
		
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
	}
	else
	{
		if (this->isShaderSupported)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
			
			glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
			
			glEnableVertexAttribArray(OGLVertexAttributeID_Position);
			glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
			glEnableVertexAttribArray(OGLVertexAttributeID_Color);
			
			glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
			glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
			glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
		}
		else
		{
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glEnableClientState(GL_VERTEX_ARRAY);
			
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
			
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glColorPointer(4, GL_FLOAT, 0, OGLRef.color4fBuffer);
			
			glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
			glVertexPointer(4, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
			glTexCoordPointer(2, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_5::DisableVertexAttributes()
{
	if (this->isVAOSupported)
	{
		glBindVertexArray(0);
	}
	else
	{
		if (this->isShaderSupported)
		{
			glDisableVertexAttribArray(OGLVertexAttributeID_Position);
			glDisableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
			glDisableVertexAttribArray(OGLVertexAttributeID_Color);
		}
		else
		{
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_5::ReadBackPixels()
{
	const size_t i = this->doubleBufferIndex;
	
	if (this->isPBOSupported)
	{
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, this->ref->pboRenderDataID[i]);
		glReadPixels(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, 0);
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	
	this->gpuScreen3DHasNewData[i] = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_5::RenderFinish()
{
	const size_t i = this->doubleBufferIndex;
	
	if (!this->gpuScreen3DHasNewData[i])
	{
		return OGLERROR_NOERR;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isPBOSupported)
	{
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, OGLRef.pboRenderDataID[i]);
		
		const u32 *__restrict mappedBufferPtr = (u32 *__restrict)glMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
		if (mappedBufferPtr != NULL)
		{
			this->ConvertFramebuffer(mappedBufferPtr, (u32 *)gfx3d_convertedScreen);
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB);
		}
		
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	else
	{
		u32 *__restrict workingBuffer = this->GPU_screen3D[i];
		glReadPixels(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, workingBuffer);
		this->ConvertFramebuffer(workingBuffer, (u32 *)gfx3d_convertedScreen);
	}
	
	this->gpuScreen3DHasNewData[i] = false;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::InitExtensions()
{
	Render3DError error = OGLERROR_NOERR;
	
	// Get OpenGL extensions
	std::set<std::string> oglExtensionSet;
	this->GetExtensionSet(&oglExtensionSet);
	
	// Initialize OpenGL
	this->InitTables();
	
	// Load and create shaders. Return on any error, since a v2.0 driver will assume that shaders are available.
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
	
	std::string edgeMarkVtxShaderString = std::string(EdgeMarkVtxShader_100);
	std::string edgeMarkFragShaderString = std::string(EdgeMarkFragShader_100);
	std::string fogVtxShaderString = std::string(FogVtxShader_100);
	std::string fogFragShaderString = std::string(FogFragShader_100);
	error = this->InitPostprocessingPrograms(edgeMarkVtxShaderString, edgeMarkFragShaderString, fogVtxShaderString, fogFragShaderString);
	if (error != OGLERROR_NOERR)
	{
		this->DestroyGeometryProgram();
		this->isShaderSupported = false;
	}
	
	this->isVBOSupported = true;
	this->CreateVBOs();
	
	this->isPBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_buffer_object") &&
							 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_pixel_buffer_object") ||
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_pixel_buffer_object"));
	if (this->isPBOSupported)
	{
		this->CreatePBOs();
	}
	
	this->isVAOSupported	= this->isShaderSupported &&
							  this->isVBOSupported &&
							 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_array_object") ||
							  this->IsExtensionPresent(&oglExtensionSet, "GL_APPLE_vertex_array_object"));
	if (this->isVAOSupported)
	{
		this->CreateVAOs();
	}
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
	this->isFBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_object") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_blit") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_packed_depth_stencil");
	if (this->isFBOSupported)
	{
		error = this->CreateFBOs();
		if (error != OGLERROR_NOERR)
		{
			this->isFBOSupported = false;
		}
	}
	else
	{
		INFO("OpenGL: FBOs are unsupported. Some emulation features will be disabled.\n");
	}
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
	this->isMultisampledFBOSupported	= this->isFBOSupported &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_multisample");
	if (this->isMultisampledFBOSupported)
	{
		error = this->CreateMultisampledFBO();
		if (error != OGLERROR_NOERR)
		{
			this->isMultisampledFBOSupported = false;
		}
	}
	else
	{
		INFO("OpenGL: Multisampled FBOs are unsupported. Multisample antialiasing will be disabled.\n");
	}
	
	this->InitTextures();
	this->InitFinalRenderStates(&oglExtensionSet); // This must be done last
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::InitFinalRenderStates(const std::set<std::string> *oglExtensionSet)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// we want to use alpha destination blending so we can track the last-rendered alpha value
	// test: new super mario brothers renders the stormclouds at the beginning
	
	// Blending Support
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_DST_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);
	
	// Mirrored Repeat Mode Support
	OGLRef.stateTexMirroredRepeat = GL_MIRRORED_REPEAT;
	
	// Always enable depth test, and control using glDepthMask().
	glEnable(GL_DEPTH_TEST);
	
	// Ignore our color buffer since we'll transfer the polygon alpha through a uniform.
	OGLRef.color4fBuffer = NULL;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::InitEdgeMarkProgramBindings()
{
	OGLRenderRef &OGLRef = *this->ref;
	glBindAttribLocation(OGLRef.programEdgeMarkID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programEdgeMarkID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::InitPostprocessingPrograms(const std::string &edgeMarkVtxShader, const std::string &edgeMarkFragShader, const std::string &fogVtxShader, const std::string &fogFragShader)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	OGLRef.vertexEdgeMarkShaderID = glCreateShader(GL_VERTEX_SHADER);
	if(!OGLRef.vertexEdgeMarkShaderID)
	{
		INFO("OpenGL: Failed to create the edge mark vertex shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *edgeMarkVtxShaderCStr = edgeMarkVtxShader.c_str();
	glShaderSource(OGLRef.vertexEdgeMarkShaderID, 1, (const GLchar **)&edgeMarkVtxShaderCStr, NULL);
	glCompileShader(OGLRef.vertexEdgeMarkShaderID);
	if (!this->ValidateShaderCompile(OGLRef.vertexEdgeMarkShaderID))
	{
		glDeleteShader(OGLRef.vertexEdgeMarkShaderID);
		INFO("OpenGL: Failed to compile the edge mark vertex shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.fragmentEdgeMarkShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if(!OGLRef.fragmentEdgeMarkShaderID)
	{
		glDeleteShader(OGLRef.vertexEdgeMarkShaderID);
		INFO("OpenGL: Failed to create the edge mark fragment shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *edgeMarkFragShaderCStr = edgeMarkFragShader.c_str();
	glShaderSource(OGLRef.fragmentEdgeMarkShaderID, 1, (const GLchar **)&edgeMarkFragShaderCStr, NULL);
	glCompileShader(OGLRef.fragmentEdgeMarkShaderID);
	if (!this->ValidateShaderCompile(OGLRef.fragmentEdgeMarkShaderID))
	{
		glDeleteShader(OGLRef.vertexEdgeMarkShaderID);
		glDeleteShader(OGLRef.fragmentEdgeMarkShaderID);
		INFO("OpenGL: Failed to compile the edge mark fragment shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.programEdgeMarkID = glCreateProgram();
	if(!OGLRef.programEdgeMarkID)
	{
		glDeleteShader(OGLRef.vertexEdgeMarkShaderID);
		glDeleteShader(OGLRef.fragmentEdgeMarkShaderID);
		INFO("OpenGL: Failed to create the edge mark shader program.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glAttachShader(OGLRef.programEdgeMarkID, OGLRef.vertexEdgeMarkShaderID);
	glAttachShader(OGLRef.programEdgeMarkID, OGLRef.fragmentEdgeMarkShaderID);
	
	error = this->InitEdgeMarkProgramBindings();
	if (error != OGLERROR_NOERR)
	{
		glDetachShader(OGLRef.programEdgeMarkID, OGLRef.vertexEdgeMarkShaderID);
		glDetachShader(OGLRef.programEdgeMarkID, OGLRef.fragmentEdgeMarkShaderID);
		glDeleteProgram(OGLRef.programEdgeMarkID);
		glDeleteShader(OGLRef.vertexEdgeMarkShaderID);
		glDeleteShader(OGLRef.fragmentEdgeMarkShaderID);
		INFO("OpenGL: Failed to make the edge mark shader bindings.\n");
		return error;
	}
	
	glLinkProgram(OGLRef.programEdgeMarkID);
	if (!this->ValidateShaderProgramLink(OGLRef.programEdgeMarkID))
	{
		glDetachShader(OGLRef.programEdgeMarkID, OGLRef.vertexEdgeMarkShaderID);
		glDetachShader(OGLRef.programEdgeMarkID, OGLRef.fragmentEdgeMarkShaderID);
		glDeleteProgram(OGLRef.programEdgeMarkID);
		glDeleteShader(OGLRef.vertexEdgeMarkShaderID);
		glDeleteShader(OGLRef.fragmentEdgeMarkShaderID);
		INFO("OpenGL: Failed to link the edge mark shader program.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.programEdgeMarkID);
	glUseProgram(OGLRef.programEdgeMarkID);
	
	// Set up shader uniforms
	OGLRef.uniformTexGColor_EdgeMark	= glGetUniformLocation(OGLRef.programEdgeMarkID, "texInFragColor");
	OGLRef.uniformTexGDepth_EdgeMark	= glGetUniformLocation(OGLRef.programEdgeMarkID, "texInFragDepth");
	OGLRef.uniformTexGPolyID_EdgeMark	= glGetUniformLocation(OGLRef.programEdgeMarkID, "texInPolyID");
	
	OGLRef.uniformFramebufferSize		= glGetUniformLocation(OGLRef.programEdgeMarkID, "framebufferSize");
	OGLRef.uniformStateEdgeColor		= glGetUniformLocation(OGLRef.programEdgeMarkID, "stateEdgeColor");
	
	// ------------------------------------------
	
	OGLRef.vertexFogShaderID = glCreateShader(GL_VERTEX_SHADER);
	if(!OGLRef.vertexFogShaderID)
	{
		INFO("OpenGL: Failed to create the fog vertex shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *fogVtxShaderCStr = fogVtxShader.c_str();
	glShaderSource(OGLRef.vertexFogShaderID, 1, (const GLchar **)&fogVtxShaderCStr, NULL);
	glCompileShader(OGLRef.vertexFogShaderID);
	if (!this->ValidateShaderCompile(OGLRef.vertexFogShaderID))
	{
		glDeleteShader(OGLRef.vertexFogShaderID);
		INFO("OpenGL: Failed to compile the fog vertex shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.fragmentFogShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if(!OGLRef.fragmentFogShaderID)
	{
		glDeleteShader(OGLRef.vertexFogShaderID);
		INFO("OpenGL: Failed to create the fog fragment shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *fogFragShaderCStr = fogFragShader.c_str();
	glShaderSource(OGLRef.fragmentFogShaderID, 1, (const GLchar **)&fogFragShaderCStr, NULL);
	glCompileShader(OGLRef.fragmentFogShaderID);
	if (!this->ValidateShaderCompile(OGLRef.fragmentFogShaderID))
	{
		glDeleteShader(OGLRef.vertexFogShaderID);
		glDeleteShader(OGLRef.fragmentFogShaderID);
		INFO("OpenGL: Failed to compile the fog fragment shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.programFogID = glCreateProgram();
	if(!OGLRef.programFogID)
	{
		glDeleteShader(OGLRef.vertexFogShaderID);
		glDeleteShader(OGLRef.fragmentFogShaderID);
		INFO("OpenGL: Failed to create the fog shader program.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glAttachShader(OGLRef.programFogID, OGLRef.vertexFogShaderID);
	glAttachShader(OGLRef.programFogID, OGLRef.fragmentFogShaderID);
	
	error = this->InitFogProgramBindings();
	if (error != OGLERROR_NOERR)
	{
		glDetachShader(OGLRef.programFogID, OGLRef.vertexFogShaderID);
		glDetachShader(OGLRef.programFogID, OGLRef.fragmentFogShaderID);
		glDeleteProgram(OGLRef.programFogID);
		glDeleteShader(OGLRef.vertexFogShaderID);
		glDeleteShader(OGLRef.fragmentFogShaderID);
		INFO("OpenGL: Failed to make the fog shader bindings.\n");
		return error;
	}
	
	glLinkProgram(OGLRef.programFogID);
	if (!this->ValidateShaderProgramLink(OGLRef.programFogID))
	{
		glDetachShader(OGLRef.programFogID, OGLRef.vertexFogShaderID);
		glDetachShader(OGLRef.programFogID, OGLRef.fragmentFogShaderID);
		glDeleteProgram(OGLRef.programFogID);
		glDeleteShader(OGLRef.vertexFogShaderID);
		glDeleteShader(OGLRef.fragmentFogShaderID);
		INFO("OpenGL: Failed to link the fog shader program.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.programFogID);
	glUseProgram(OGLRef.programFogID);
	
	// Set up shader uniforms
	OGLRef.uniformTexGColor_Fog			= glGetUniformLocation(OGLRef.programFogID, "texInFragColor");
	OGLRef.uniformTexGDepth_Fog			= glGetUniformLocation(OGLRef.programFogID, "texInFragDepth");
	OGLRef.uniformTexGFog_Fog			= glGetUniformLocation(OGLRef.programFogID, "texInFogAttributes");
	
	OGLRef.uniformStateFogColor			= glGetUniformLocation(OGLRef.programFogID, "stateFogColor");
	OGLRef.uniformStateFogDensity		= glGetUniformLocation(OGLRef.programFogID, "stateFogDensity");
	OGLRef.uniformStateFogOffset		= glGetUniformLocation(OGLRef.programFogID, "stateFogOffset");
	OGLRef.uniformStateFogStep			= glGetUniformLocation(OGLRef.programFogID, "stateFogStep");
	
	glUseProgram(OGLRef.programGeometryID);
	INFO("OpenGL: Successfully created postprocess shaders.\n");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::InitFogProgramBindings()
{
	OGLRenderRef &OGLRef = *this->ref;
	glBindAttribLocation(OGLRef.programFogID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programFogID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::DestroyPostprocessingPrograms()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glUseProgram(0);
	glDetachShader(OGLRef.programEdgeMarkID, OGLRef.vertexEdgeMarkShaderID);
	glDetachShader(OGLRef.programEdgeMarkID, OGLRef.fragmentEdgeMarkShaderID);
	glDetachShader(OGLRef.programFogID, OGLRef.vertexFogShaderID);
	glDetachShader(OGLRef.programFogID, OGLRef.fragmentFogShaderID);
	
	glDeleteProgram(OGLRef.programEdgeMarkID);
	glDeleteProgram(OGLRef.programFogID);
	
	glDeleteShader(OGLRef.vertexEdgeMarkShaderID);
	glDeleteShader(OGLRef.fragmentEdgeMarkShaderID);
	glDeleteShader(OGLRef.vertexFogShaderID);
	glDeleteShader(OGLRef.fragmentFogShaderID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::SetupVertices(const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList, GLushort *outIndexBuffer, size_t *outIndexCount)
{
	const size_t polyCount = polyList->count;
	size_t vertIndexCount = 0;
	
	for(size_t i = 0; i < polyCount; i++)
	{
		const POLY *poly = &polyList->list[indexList->list[i]];
		const size_t polyType = poly->type;
		
		for(size_t j = 0; j < polyType; j++)
		{
			const GLushort vertIndex = poly->vertIndexes[j];
			
			// While we're looping through our vertices, add each vertex index to
			// a buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
			// vertices here to convert them to GL_TRIANGLES, which are much easier
			// to work with and won't be deprecated in future OpenGL versions.
			outIndexBuffer[vertIndexCount++] = vertIndex;
			if (poly->vtxFormat == GFX3D_QUADS || poly->vtxFormat == GFX3D_QUAD_STRIP)
			{
				if (j == 2)
				{
					outIndexBuffer[vertIndexCount++] = vertIndex;
				}
				else if (j == 3)
				{
					outIndexBuffer[vertIndexCount++] = poly->vertIndexes[0];
				}
			}
		}
	}
	
	*outIndexCount = vertIndexCount;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const size_t vertIndexCount)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(OGLRef.vaoGeometryStatesID);
		glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
		
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
	}
	else
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
		
		glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
		
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
		glEnableVertexAttribArray(OGLVertexAttributeID_Color);
		
		glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
		glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::DisableVertexAttributes()
{
	if (this->isVAOSupported)
	{
		glBindVertexArray(0);
	}
	else
	{
		glDisableVertexAttribArray(OGLVertexAttributeID_Position);
		glDisableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
		glDisableVertexAttribArray(OGLVertexAttributeID_Color);
		
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::BeginRender(const GFX3D_State *renderState)
{
	OGLRenderRef &OGLRef = *this->ref;
	this->doubleBufferIndex = (this->doubleBufferIndex + 1) & 0x01;
	
	this->SelectRenderingFramebuffer();
	
	glUseProgram(OGLRef.programGeometryID);
	glUniform1i(OGLRef.uniformStateToonShadingMode, renderState->shading);
	glUniform1i(OGLRef.uniformStateEnableAlphaTest, (renderState->enableAlphaTest) ? GL_TRUE : GL_FALSE);
	glUniform1i(OGLRef.uniformStateEnableAntialiasing, (renderState->enableAntialiasing) ? GL_TRUE : GL_FALSE);
	glUniform1i(OGLRef.uniformStateEnableEdgeMarking, (renderState->enableEdgeMarking) ? GL_TRUE : GL_FALSE);
	glUniform1i(OGLRef.uniformStateEnableFogAlphaOnly, (renderState->enableFogAlphaOnly) ? GL_TRUE : GL_FALSE);
	glUniform1i(OGLRef.uniformStateUseWDepth, (renderState->wbuffer) ? GL_TRUE : GL_FALSE);
	glUniform1f(OGLRef.uniformStateAlphaTestRef, divide5bitBy31_LUT[renderState->alphaTestRef]);
	glUniform1i(OGLRef.uniformTexRenderObject, 0);
	glUniform1i(OGLRef.uniformTexToonTable, OGLTextureUnitID_ToonTable);
	
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);
	
	if(renderState->enableAlphaBlending)
	{
		glEnable(GL_BLEND);
	}
	else
	{
		glDisable(GL_BLEND);
	}
	
	glDepthMask(GL_TRUE);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::RenderEdgeMarking(const u16 *colorTable)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	const GLfloat oglColor[4*8]	= {divide5bitBy31_LUT[(colorTable[0]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[0] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[0] >> 10) & 0x001F],
								   1.0f,
								   divide5bitBy31_LUT[(colorTable[1]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[1] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[1] >> 10) & 0x001F],
								   1.0f,
								   divide5bitBy31_LUT[(colorTable[2]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[2] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[2] >> 10) & 0x001F],
								   1.0f,
								   divide5bitBy31_LUT[(colorTable[3]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[3] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[3] >> 10) & 0x001F],
								   1.0f,
								   divide5bitBy31_LUT[(colorTable[4]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[4] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[4] >> 10) & 0x001F],
								   1.0f,
								   divide5bitBy31_LUT[(colorTable[5]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[5] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[5] >> 10) & 0x001F],
								   1.0f,
								   divide5bitBy31_LUT[(colorTable[6]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[6] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[6] >> 10) & 0x001F],
								   1.0f,
								   divide5bitBy31_LUT[(colorTable[7]      ) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[7] >>  5) & 0x001F],
								   divide5bitBy31_LUT[(colorTable[7] >> 10) & 0x001F],
								   1.0f};
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboPostprocessID);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	glUseProgram(OGLRef.programEdgeMarkID);
	glUniform1i(OGLRef.uniformTexGColor_EdgeMark, OGLTextureUnitID_GColor);
	glUniform1i(OGLRef.uniformTexGDepth_EdgeMark, OGLTextureUnitID_GDepth);
	glUniform1i(OGLRef.uniformTexGPolyID_EdgeMark, OGLTextureUnitID_GPolyID);
	glUniform2f(OGLRef.uniformFramebufferSize, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glUniform4fv(OGLRef.uniformStateEdgeColor, 8, oglColor);
	
	glViewport(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboPostprocessIndexID);
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(OGLRef.vaoPostprocessStatesID);
	}
	else
	{
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
		glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(sizeof(GLfloat) * 8));
	}
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GColor);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGColorID);
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GDepth);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthID);
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GPolyID);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGPolyID);
	glActiveTexture(GL_TEXTURE0);
	
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(0);
	}
	else
	{
		glDisableVertexAttribArray(OGLVertexAttributeID_Position);
		glDisableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	}
	
	this->didRenderEdgeMark = true;
	
	return RENDER3DERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::RenderFog(const u8 *densityTable, const u32 color, const u32 offset, const u8 shift)
{
	OGLRenderRef &OGLRef = *this->ref;
	static GLfloat oglDensityTable[32];
	
	if (!this->isFBOSupported)
	{
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	
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
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboPostprocessID);
	glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);
	glReadBuffer(GL_COLOR_ATTACHMENT1_EXT);
	glUseProgram(OGLRef.programFogID);
	glUniform1i(OGLRef.uniformTexGColor_Fog, OGLTextureUnitID_GColor);
	glUniform1i(OGLRef.uniformTexGDepth_Fog, OGLTextureUnitID_GDepth);
	glUniform1i(OGLRef.uniformTexGFog_Fog, OGLTextureUnitID_FogAttr);
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
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(OGLRef.vaoPostprocessStatesID);
	}
	else
	{
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
		glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(sizeof(GLfloat) * 8));
	}
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GColor);
	glBindTexture(GL_TEXTURE_2D, (this->didRenderEdgeMark) ? OGLRef.texPostprocessEdgeMarkID : OGLRef.texGColorID);
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GDepth);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthID);
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FogAttr);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGFogAttrID);
	glActiveTexture(GL_TEXTURE0);
	
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(0);
	}
	else
	{
		glDisableVertexAttribArray(OGLVertexAttributeID_Position);
		glDisableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::SetupPolygon(const POLY *thePoly)
{
	OGLRenderRef &OGLRef = *this->ref;
	const PolygonAttributes attr = thePoly->getAttributes();
	
	// Set up polygon attributes
	glUniform1i(OGLRef.uniformPolyMode, attr.polygonMode);
	glUniform1i(OGLRef.uniformPolyEnableFog, (attr.enableRenderFog) ? GL_TRUE : GL_FALSE);
	glUniform1f(OGLRef.uniformPolyAlpha, (!attr.isWireframe && attr.isTranslucent) ? divide5bitBy31_LUT[attr.alpha] : 1.0f);
	glUniform1i(OGLRef.uniformPolyID, attr.polygonID);
	
	// Set up depth test mode
	static const GLenum oglDepthFunc[2] = {GL_LESS, GL_EQUAL};
	glDepthFunc(oglDepthFunc[attr.enableDepthTest]);
	
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
	if(attr.polygonMode == 3)
	{
		glEnable(GL_STENCIL_TEST);
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
		glEnable(GL_STENCIL_TEST);
		if(attr.isTranslucent)
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
	
	if(attr.isTranslucent && !attr.enableAlphaDepthWrite)
	{
		enableDepthWrite = GL_FALSE;
	}
	
	glDepthMask(enableDepthWrite);
	glUniform1i(OGLRef.uniformPolyEnableDepthWrite, enableDepthWrite);
	glUniform1i(OGLRef.uniformPolySetNewDepthForTranslucent, (attr.enableAlphaDepthWrite) ? GL_TRUE : GL_FALSE);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::SetupTexture(const POLY *thePoly, bool enableTexturing)
{
	OGLRenderRef &OGLRef = *this->ref;
	const PolygonTexParams params = thePoly->getTexParams();
	
	// Check if we need to use textures
	if (thePoly->texParam == 0 || params.texFormat == TEXMODE_NONE || !enableTexturing)
	{
		glUniform1i(OGLRef.uniformPolyEnableTexture, GL_FALSE);
		return OGLERROR_NOERR;
	}
	
	glUniform1i(OGLRef.uniformPolyEnableTexture, GL_TRUE);
	
	//	texCacheUnit.TexCache_SetTexture<TexFormat_32bpp>(format, texpal);
	TexCacheItem *newTexture = TexCache_SetTexture(TexFormat_32bpp, thePoly->texParam, thePoly->texPalette);
	if(newTexture != this->currTexture)
	{
		this->currTexture = newTexture;
		//has the ogl renderer initialized the texture?
		if(!this->currTexture->deleteCallback)
		{
			this->currTexture->deleteCallback = texDeleteCallback;
			
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
		
		glUniform2f(OGLRef.uniformPolyTexScale, this->currTexture->invSizeX, this->currTexture->invSizeY);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_1::ReadBackPixels()
{
	const size_t i = this->doubleBufferIndex;
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER, this->ref->pboRenderDataID[i]);
	glReadPixels(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, 0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	
	this->gpuScreen3DHasNewData[i] = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_1::RenderFinish()
{
	const size_t i = this->doubleBufferIndex;
	
	if (!this->gpuScreen3DHasNewData[i])
	{
		return OGLERROR_NOERR;
	}
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER, this->ref->pboRenderDataID[i]);
	
	const u32 *__restrict mappedBufferPtr = (u32 *__restrict)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
	if (mappedBufferPtr != NULL)
	{
		this->ConvertFramebuffer(mappedBufferPtr, (u32 *)gfx3d_convertedScreen);
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	}
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	
	this->gpuScreen3DHasNewData[i] = false;
	
	return OGLERROR_NOERR;
}
