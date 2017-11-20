/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2017 DeSmuME team

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
#include <algorithm>

#include "common.h"
#include "debug.h"
#include "NDSSystem.h"

#include "./filter/filter.h"
#include "./filter/xbrz.h"

#ifdef ENABLE_SSE2
#include <emmintrin.h>
#include "./utils/colorspacehandler/colorspacehandler_SSE2.h"
#endif

#if MSB_FIRST
	#define GL_TEXTURE_SRC_FORMAT GL_UNSIGNED_INT_8_8_8_8
#else
	#define GL_TEXTURE_SRC_FORMAT GL_UNSIGNED_INT_8_8_8_8_REV
#endif

typedef struct
{
	unsigned int major;
	unsigned int minor;
	unsigned int revision;
} OGLVersion;

static OGLVersion _OGLDriverVersion = {0, 0, 0};

// Lookup Tables
static CACHE_ALIGN GLfloat material_8bit_to_float[256] = {0};
CACHE_ALIGN const GLfloat divide5bitBy31_LUT[32]	= {0.0,             0.0322580645161, 0.0645161290323, 0.0967741935484,
													   0.1290322580645, 0.1612903225806, 0.1935483870968, 0.2258064516129,
													   0.2580645161290, 0.2903225806452, 0.3225806451613, 0.3548387096774,
													   0.3870967741935, 0.4193548387097, 0.4516129032258, 0.4838709677419,
													   0.5161290322581, 0.5483870967742, 0.5806451612903, 0.6129032258065,
													   0.6451612903226, 0.6774193548387, 0.7096774193548, 0.7419354838710,
													   0.7741935483871, 0.8064516129032, 0.8387096774194, 0.8709677419355,
													   0.9032258064516, 0.9354838709677, 0.9677419354839, 1.0};


CACHE_ALIGN const GLfloat divide6bitBy63_LUT[64]	= {0.0,             0.0158730158730, 0.0317460317460, 0.0476190476191,
													   0.0634920634921, 0.0793650793651, 0.0952380952381, 0.1111111111111,
													   0.1269841269841, 0.1428571428571, 0.1587301587302, 0.1746031746032,
													   0.1904761904762, 0.2063492063492, 0.2222222222222, 0.2380952380952,
													   0.2539682539683, 0.2698412698413, 0.2857142857143, 0.3015873015873,
													   0.3174603174603, 0.3333333333333, 0.3492063492064, 0.3650793650794,
													   0.3809523809524, 0.3968253968254, 0.4126984126984, 0.4285714285714,
													   0.4444444444444, 0.4603174603175, 0.4761904761905, 0.4920634920635,
													   0.5079365079365, 0.5238095238095, 0.5396825396825, 0.5555555555556,
													   0.5714285714286, 0.5873015873016, 0.6031746031746, 0.6190476190476,
													   0.6349206349206, 0.6507936507937, 0.6666666666667, 0.6825396825397,
													   0.6984126984127, 0.7142857142857, 0.7301587301587, 0.7460317460318,
													   0.7619047619048, 0.7777777777778, 0.7936507936508, 0.8095238095238,
													   0.8253968253968, 0.8412698412698, 0.8571428571429, 0.8730158730159,
													   0.8888888888889, 0.9047619047619, 0.9206349206349, 0.9365079365079,
													   0.9523809523810, 0.9682539682540, 0.9841269841270, 1.0};

const GLfloat PostprocessVtxBuffer[16]	= {-1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,
										    0.0f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f,  0.0f,  1.0f};
const GLubyte PostprocessElementBuffer[6] = {0, 1, 2, 2, 3, 0};

const GLenum RenderDrawList[3] = {GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_COLOR_ATTACHMENT2_EXT};

bool BEGINGL()
{
	if(oglrender_beginOpenGL) 
		return oglrender_beginOpenGL();
	else return true;
}

void ENDGL()
{
	if(oglrender_endOpenGL) 
		oglrender_endOpenGL();
}

// Function Pointers
bool (*oglrender_init)() = NULL;
bool (*oglrender_beginOpenGL)() = NULL;
void (*oglrender_endOpenGL)() = NULL;
bool (*oglrender_framebufferDidResizeCallback)(size_t w, size_t h) = NULL;
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
static const char *GeometryVtxShader_100 = {"\
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
static const char *GeometryFragShader_100 = {"\
	#define DEPTH_EQUALS_TEST_TOLERANCE 255.0\n\
	\n\
	varying vec4 vtxPosition;\n\
	varying vec2 vtxTexCoord;\n\
	varying vec4 vtxColor;\n\
	\n\
	uniform sampler2D texRenderObject;\n\
	uniform sampler1D texToonTable;\n\
	\n\
	uniform int stateToonShadingMode;\n\
	uniform bool stateEnableAlphaTest;\n\
	uniform bool stateEnableAntialiasing;\n\
	uniform bool stateEnableEdgeMarking;\n\
	uniform bool stateUseWDepth;\n\
	uniform float stateAlphaTestRef;\n\
	\n\
	uniform int polyMode;\n\
	uniform bool polyIsWireframe;\n\
	uniform bool polySetNewDepthForTranslucent;\n\
	uniform int polyID;\n\
	\n\
	uniform bool polyEnableTexture;\n\
	uniform bool polyEnableFog;\n\
	uniform bool texDrawOpaque;\n\
	uniform bool texSingleBitAlpha;\n\
	\n\
	uniform bool polyDrawShadow;\n\
	uniform int polyDepthOffsetMode;\n\
	\n\
	void main()\n\
	{\n\
		vec4 newFragColor = vec4(0.0, 0.0, 0.0, 0.0);\n\
		vec4 newPolyID = vec4(0.0, 0.0, 0.0, 0.0);\n\
		vec4 newFogAttributes = vec4(0.0, 0.0, 0.0, 0.0);\n\
		\n\
		float vertW = (vtxPosition.w == 0.0) ? 0.00000001 : vtxPosition.w;\n\
		float depthOffset = (polyDepthOffsetMode == 0) ? 0.0 : ((polyDepthOffsetMode == 1) ? -DEPTH_EQUALS_TEST_TOLERANCE : DEPTH_EQUALS_TEST_TOLERANCE);\n\
		// hack: when using z-depth, drop some LSBs so that the overworld map in Dragon Quest IV shows up correctly\n\
		float newFragDepthValue = (stateUseWDepth) ? clamp( ( floor(vtxPosition.w * 4096.0) + depthOffset ) / 16777215.0, 0.0, 1.0 ) : clamp( ( (floor(clamp(((vtxPosition.z/vertW) * 0.5 + 0.5), 0.0, 1.0) * 32767.0) * 512.0) + depthOffset ) / 16777215.0, 0.0, 1.0 );\n\
		\n\
		if ((polyMode != 3) || polyDrawShadow)\n\
		{\n\
			vec4 mainTexColor = (polyEnableTexture) ? texture2D(texRenderObject, vtxTexCoord) : vec4(1.0, 1.0, 1.0, 1.0);\n\
			\n\
			if (texSingleBitAlpha)\n\
			{\n\
				if (mainTexColor.a < 0.500)\n\
				{\n\
					mainTexColor.a = 0.0;\n\
				}\n\
				else\n\
				{\n\
					mainTexColor.rgb = mainTexColor.rgb / mainTexColor.a;\n\
					mainTexColor.a = 1.0;\n\
				}\n\
			}\n\
			else\n\
			{\n\
				if (texDrawOpaque)\n\
				{\n\
					if ( (polyMode != 1) && (mainTexColor.a <= 0.999) )\n\
					{\n\
						discard;\n\
					}\n\
				}\n\
			}\n\
			\n\
			newFragColor = mainTexColor * vtxColor;\n\
			\n\
			if (polyMode == 1)\n\
			{\n\
				newFragColor.rgb = (polyEnableTexture) ? mix(vtxColor.rgb, mainTexColor.rgb, mainTexColor.a) : vtxColor.rgb;\n\
				newFragColor.a = vtxColor.a;\n\
			}\n\
			else if (polyMode == 2)\n\
			{\n\
				vec3 newToonColor = vec3(texture1D(texToonTable, vtxColor.r).rgb);\n\
				newFragColor.rgb = (stateToonShadingMode == 0) ? mainTexColor.rgb * newToonColor.rgb : min((mainTexColor.rgb * vtxColor.r) + newToonColor.rgb, 1.0);\n\
			}\n\
			else if (polyMode == 3)\n\
			{\n\
				newFragColor = vtxColor;\n\
			}\n\
			\n\
			if (newFragColor.a < 0.001 || (stateEnableAlphaTest && newFragColor.a < stateAlphaTestRef))\n\
			{\n\
				discard;\n\
			}\n\
			\n\
			newPolyID = vec4( float(polyID)/63.0, float(polyIsWireframe), 0.0, float(newFragColor.a > 0.999) );\n\
			newFogAttributes = vec4( float(polyEnableFog), 0.0, 0.0, float((newFragColor.a > 0.999) ? 1.0 : 0.5) );\n\
		}\n\
		\n\
		gl_FragData[0] = newFragColor;\n\
		gl_FragData[1] = newPolyID;\n\
		gl_FragData[2] = newFogAttributes;\n\
		gl_FragDepth = newFragDepthValue;\n\
	}\n\
"};

// Vertex shader for determining which pixels have a zero alpha, GLSL 1.00
static const char *GeometryZeroDstAlphaPixelMaskVtxShader_100 = {"\
	attribute vec2 inPosition;\n\
	attribute vec2 inTexCoord0;\n\
	varying vec2 texCoord;\n\
	\n\
	void main()\n\
	{\n\
		texCoord = inTexCoord0;\n\
		gl_Position = vec4(inPosition, 0.0, 1.0);\n\
	}\n\
"};

// Fragment shader for determining which pixels have a zero alpha, GLSL 1.00
static const char *GeometryZeroDstAlphaPixelMaskFragShader_100 = {"\
	varying vec2 texCoord;\n\
	uniform sampler2D texInFragColor;\n\
	\n\
	void main()\n\
	{\n\
		vec4 inFragColor = texture2D(texInFragColor, texCoord);\n\
		\n\
		if (inFragColor.a <= 0.001)\n\
		{\n\
			discard;\n\
		}\n\
	}\n\
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
	uniform sampler2D texInFragDepth;\n\
	uniform sampler2D texInPolyID;\n\
	uniform vec4 stateEdgeColor[8];\n\
	\n\
	void main()\n\
	{\n\
		vec4 polyIDInfo[5];\n\
		polyIDInfo[0] = texture2D(texInPolyID, texCoord[0]);\n\
		polyIDInfo[1] = texture2D(texInPolyID, texCoord[1]);\n\
		polyIDInfo[2] = texture2D(texInPolyID, texCoord[2]);\n\
		polyIDInfo[3] = texture2D(texInPolyID, texCoord[3]);\n\
		polyIDInfo[4] = texture2D(texInPolyID, texCoord[4]);\n\
		\n\
		bool isWireframe[5];\n\
		isWireframe[0] = bool(polyIDInfo[0].g);\n\
		\n\
		float depth[5];\n\
		depth[0] = texture2D(texInFragDepth, texCoord[0]).r;\n\
		depth[1] = texture2D(texInFragDepth, texCoord[1]).r;\n\
		depth[2] = texture2D(texInFragDepth, texCoord[2]).r;\n\
		depth[3] = texture2D(texInFragDepth, texCoord[3]).r;\n\
		depth[4] = texture2D(texInFragDepth, texCoord[4]).r;\n\
		\n\
		vec4 newEdgeColor = vec4(0.0, 0.0, 0.0, 0.0);\n\
		\n\
		if (!isWireframe[0])\n\
		{\n\
			int polyID[5];\n\
			polyID[0] = int((polyIDInfo[0].r * 63.0) + 0.5);\n\
			polyID[1] = int((polyIDInfo[1].r * 63.0) + 0.5);\n\
			polyID[2] = int((polyIDInfo[2].r * 63.0) + 0.5);\n\
			polyID[3] = int((polyIDInfo[3].r * 63.0) + 0.5);\n\
			polyID[4] = int((polyIDInfo[4].r * 63.0) + 0.5);\n\
			\n\
			isWireframe[1] = bool(polyIDInfo[1].g);\n\
			isWireframe[2] = bool(polyIDInfo[2].g);\n\
			isWireframe[3] = bool(polyIDInfo[3].g);\n\
			isWireframe[4] = bool(polyIDInfo[4].g);\n\
			\n\
			for (int i = 1; i < 5; i++)\n\
			{\n\
				if ( (polyID[0] != polyID[i]) && (depth[0] >= depth[i]) && !isWireframe[i] )\n\
				{\n\
					newEdgeColor = stateEdgeColor[polyID[i]/8];\n\
					break;\n\
				}\n\
			}\n\
		}\n\
		\n\
		gl_FragData[0] = newEdgeColor;\n\
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
	uniform bool stateEnableFogAlphaOnly;\n\
	uniform vec4 stateFogColor;\n\
	uniform float stateFogDensity[32];\n\
	uniform float stateFogOffset;\n\
	uniform float stateFogStep;\n\
	\n\
	void main()\n\
	{\n\
		vec4 inFragColor = texture2D(texInFragColor, texCoord);\n\
		vec4 inFogAttributes = texture2D(texInFogAttributes, texCoord);\n\
		bool polyEnableFog = (inFogAttributes.r > 0.999);\n\
		vec4 newFoggedColor = inFragColor;\n\
		\n\
		if (polyEnableFog)\n\
		{\n\
			float inFragDepth = texture2D(texInFragDepth, texCoord).r;\n\
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
		gl_FragData[0] = newFoggedColor;\n\
	}\n\
"};

// Vertex shader for the final framebuffer, GLSL 1.00
static const char *FramebufferOutputVtxShader_100 = {"\
	attribute vec2 inPosition;\n\
	attribute vec2 inTexCoord0;\n\
	uniform vec2 framebufferSize;\n\
	varying vec2 texCoord;\n\
	\n\
	void main()\n\
	{\n\
		texCoord = vec2(inTexCoord0.x, (framebufferSize.y - (framebufferSize.y * inTexCoord0.y)) / framebufferSize.y);\n\
		gl_Position = vec4(inPosition, 0.0, 1.0);\n\
	}\n\
"};

// Fragment shader for the final RGBA6665 formatted framebuffer, GLSL 1.00
static const char *FramebufferOutputRGBA6665FragShader_100 = {"\
	varying vec2 texCoord;\n\
	\n\
	uniform sampler2D texInFragColor;\n\
	\n\
	void main()\n\
	{\n\
		// Note that we swap B and R since pixel readbacks are done in BGRA format for fastest\n\
		// performance. The final color is still in RGBA format.\n\
		vec4 colorRGBA6665 = texture2D(texInFragColor, texCoord).bgra;\n\
		colorRGBA6665     = floor((colorRGBA6665 * 255.0) + 0.5);\n\
		colorRGBA6665.rgb = floor(colorRGBA6665.rgb / 4.0);\n\
		colorRGBA6665.a   = floor(colorRGBA6665.a   / 8.0);\n\
		\n\
		gl_FragData[0] = (colorRGBA6665 / 255.0);\n\
	}\n\
"};

// Fragment shader for the final RGBA8888 formatted framebuffer, GLSL 1.00
static const char *FramebufferOutputRGBA8888FragShader_100 = {"\
	varying vec2 texCoord;\n\
	\n\
	uniform sampler2D texInFragColor;\n\
	\n\
	void main()\n\
	{\n\
		// Note that we swap B and R since pixel readbacks are done in BGRA format for fastest\n\
		// performance. The final color is still in RGBA format.\n\
		gl_FragData[0] = texture2D(texInFragColor, texCoord).bgra;\n\
	}\n\
"};

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

OpenGLTexture::OpenGLTexture(TEXIMAGE_PARAM texAttributes, u32 palAttributes) : Render3DTexture(texAttributes, palAttributes)
{
	_cacheSize = GetUnpackSizeUsingFormat(TexFormat_32bpp);
	_invSizeS = 1.0f / (float)_sizeS;
	_invSizeT = 1.0f / (float)_sizeT;
	
	_upscaleBuffer = NULL;
	
	glGenTextures(1, &_texID);
}

OpenGLTexture::~OpenGLTexture()
{
	glDeleteTextures(1, &this->_texID);
}

void OpenGLTexture::Load(bool isNewTexture)
{
	u32 *textureSrc = (u32 *)this->_deposterizeSrcSurface.Surface;
	
	this->Unpack<TexFormat_32bpp>(textureSrc);
	
	if (this->_useDeposterize)
	{
		RenderDeposterize(this->_deposterizeSrcSurface, this->_deposterizeDstSurface);
	}
	
	glBindTexture(GL_TEXTURE_2D, this->_texID);
	
	switch (this->_scalingFactor)
	{
		case 1:
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
			
			if (isNewTexture)
			{
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_sizeS, this->_sizeT, 0, GL_RGBA, GL_TEXTURE_SRC_FORMAT, textureSrc);
			}
			else
			{
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, this->_sizeS, this->_sizeT, GL_RGBA, GL_TEXTURE_SRC_FORMAT, textureSrc);
			}
			break;
		}
			
		case 2:
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
			
			this->_Upscale<2>(textureSrc, this->_upscaleBuffer);
			
			if (isNewTexture)
			{
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_sizeS*2, this->_sizeT*2, 0, GL_RGBA, GL_TEXTURE_SRC_FORMAT, this->_upscaleBuffer);
				glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, this->_sizeS*1, this->_sizeT*1, 0, GL_RGBA, GL_TEXTURE_SRC_FORMAT, textureSrc);
			}
			else
			{
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, this->_sizeS*2, this->_sizeT*2, GL_RGBA, GL_TEXTURE_SRC_FORMAT, this->_upscaleBuffer);
				glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, this->_sizeS*1, this->_sizeT*1, GL_RGBA, GL_TEXTURE_SRC_FORMAT, textureSrc);
			}
			break;
		}
			
		case 4:
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2);
			
			this->_Upscale<4>(textureSrc, this->_upscaleBuffer);
			
			if (isNewTexture)
			{
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_sizeS*4, this->_sizeT*4, 0, GL_RGBA, GL_TEXTURE_SRC_FORMAT, this->_upscaleBuffer);
				
				this->_Upscale<2>(textureSrc, this->_upscaleBuffer);
				glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, this->_sizeS*2, this->_sizeT*2, 0, GL_RGBA, GL_TEXTURE_SRC_FORMAT, this->_upscaleBuffer);
				
				glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, this->_sizeS*1, this->_sizeT*1, 0, GL_RGBA, GL_TEXTURE_SRC_FORMAT, textureSrc);
			}
			else
			{
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, this->_sizeS*4, this->_sizeT*4, GL_RGBA, GL_TEXTURE_SRC_FORMAT, this->_upscaleBuffer);
				
				this->_Upscale<2>(textureSrc, this->_upscaleBuffer);
				glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, this->_sizeS*2, this->_sizeT*2, GL_RGBA, GL_TEXTURE_SRC_FORMAT, this->_upscaleBuffer);
				
				glTexSubImage2D(GL_TEXTURE_2D, 2, 0, 0, this->_sizeS*1, this->_sizeT*1, GL_RGBA, GL_TEXTURE_SRC_FORMAT, textureSrc);
			}
			break;
		}
			
		default:
			break;
	}
	
	this->_isLoadNeeded = false;
}

GLuint OpenGLTexture::GetID() const
{
	return this->_texID;
}

GLfloat OpenGLTexture::GetInvWidth() const
{
	return this->_invSizeS;
}

GLfloat OpenGLTexture::GetInvHeight() const
{
	return this->_invSizeT;
}

void OpenGLTexture::SetUnpackBuffer(void *unpackBuffer)
{
	this->_deposterizeSrcSurface.Surface = (unsigned char *)unpackBuffer;
}

void OpenGLTexture::SetDeposterizeBuffer(void *dstBuffer, void *workingBuffer)
{
	this->_deposterizeDstSurface.Surface = (unsigned char *)dstBuffer;
	this->_deposterizeDstSurface.workingSurface[0] = (unsigned char *)workingBuffer;
}

void OpenGLTexture::SetUpscalingBuffer(void *upscaleBuffer)
{
	this->_upscaleBuffer = (u32 *)upscaleBuffer;
}

template<bool require_profile, bool enable_3_2>
static Render3D* OpenGLRendererCreate()
{
	OpenGLRenderer *newRenderer = NULL;
	Render3DError error = OGLERROR_NOERR;
	
	if (oglrender_init == NULL)
	{
		return NULL;
	}
	
	if (!oglrender_init())
	{
		return NULL;
	}
	
	if (!BEGINGL())
	{
		INFO("OpenGL<%s,%s>: Could not initialize -- BEGINGL() failed.\n",require_profile?"force":"auto",enable_3_2?"3_2":"old");
		return NULL;
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
		INFO("OpenGL: Incompatible graphic card detected. Disabling OpenGL support.\n");
		
		ENDGL();
		return newRenderer;
	}
	
	// Check the driver's OpenGL version
	OGLGetDriverVersion(oglVersionString, &_OGLDriverVersion.major, &_OGLDriverVersion.minor, &_OGLDriverVersion.revision);
	
	if (!IsVersionSupported(OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MAJOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MINOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_REVISION))
	{
		INFO("OpenGL: Driver does not support OpenGL v%u.%u.%u or later. Disabling 3D renderer.\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
			 OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MAJOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MINOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_REVISION,
			 oglVersionString, oglVendorString, oglRendererString);
		
		ENDGL();
		return newRenderer;
	}
	
	// Create new OpenGL rendering object
	if (enable_3_2)
	{
		if (OGLLoadEntryPoints_3_2_Func != NULL && OGLCreateRenderer_3_2_Func != NULL)
		{
			OGLLoadEntryPoints_3_2_Func();
			OGLLoadEntryPoints_Legacy(); //zero 04-feb-2013 - this seems to be necessary as well
			OGLCreateRenderer_3_2_Func(&newRenderer);
		}
		else 
		{
			if(require_profile)
			{
				ENDGL();
				return newRenderer;
			}
		}
	}
	
	// If the renderer doesn't initialize with OpenGL v3.2 or higher, fall back
	// to one of the lower versions.
	if (newRenderer == NULL)
	{
		OGLLoadEntryPoints_Legacy();
		
		if (IsVersionSupported(2, 1, 0))
		{
			newRenderer = new OpenGLRenderer_2_1;
			newRenderer->SetVersion(2, 1, 0);
		}
		else if (IsVersionSupported(2, 0, 0))
		{
			newRenderer = new OpenGLRenderer_2_0;
			newRenderer->SetVersion(2, 0, 0);
		}
		else if (IsVersionSupported(1, 2, 0))
		{
			newRenderer = new OpenGLRenderer_1_2;
			newRenderer->SetVersion(1, 2, 0);
		}
	}
	
	if (newRenderer == NULL)
	{
		INFO("OpenGL: Renderer did not initialize. Disabling 3D renderer.\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
			 oglVersionString, oglVendorString, oglRendererString);
		
		ENDGL();
		return newRenderer;
	}
	
	// Initialize OpenGL extensions
	error = newRenderer->InitExtensions();
	if (error != OGLERROR_NOERR)
	{
		if (error == OGLERROR_DRIVER_VERSION_TOO_OLD)
		{
			INFO("OpenGL: This driver does not support the minimum feature set required to run this renderer. Disabling 3D renderer.\n");
		}
		else if ( IsVersionSupported(2, 0, 0) &&
			(error == OGLERROR_SHADER_CREATE_ERROR ||
			 error == OGLERROR_VERTEX_SHADER_PROGRAM_LOAD_ERROR ||
			 error == OGLERROR_FRAGMENT_SHADER_PROGRAM_LOAD_ERROR) )
		{
			INFO("OpenGL: Shaders are not working, even though they should be. Disabling 3D renderer.\n");
		}
		else if (IsVersionSupported(3, 0, 0) && error == OGLERROR_FBO_CREATE_ERROR && OGLLoadEntryPoints_3_2_Func != NULL)
		{
			INFO("OpenGL: FBOs are not working, even though they should be. Disabling 3D renderer.\n");
		}
		
		delete newRenderer;
		newRenderer = NULL;
		
		ENDGL();
		return newRenderer;
	}
	
	ENDGL();
	
	// Initialization finished -- reset the renderer
	newRenderer->Reset();
	
	unsigned int major = 0;
	unsigned int minor = 0;
	unsigned int revision = 0;
	newRenderer->GetVersion(&major, &minor, &revision);
	
	INFO("OpenGL: Renderer initialized successfully (v%u.%u.%u).\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
		 major, minor, revision, oglVersionString, oglVendorString, oglRendererString);
	
	return newRenderer;
}

static void OpenGLRendererDestroy()
{
	if(!BEGINGL())
		return;
	
	if (CurrentRenderer != BaseRenderer)
	{
		OpenGLRenderer *oldRenderer = (OpenGLRenderer *)CurrentRenderer;
		CurrentRenderer = BaseRenderer;
		delete oldRenderer;
	}
	
	ENDGL();
}

//automatically select 3.2 or old profile depending on whether 3.2 is available
GPU3DInterface gpu3Dgl = {
	"OpenGL",
	OpenGLRendererCreate<false,true>,
	OpenGLRendererDestroy
};

//forcibly use old profile
GPU3DInterface gpu3DglOld = {
	"OpenGL Old",
	OpenGLRendererCreate<true,false>,
	OpenGLRendererDestroy
};

//forcibly use new profile
GPU3DInterface gpu3Dgl_3_2 = {
	"OpenGL 3.2",
	OpenGLRendererCreate<true,true>,
	OpenGLRendererDestroy
};

OpenGLRenderer::OpenGLRenderer()
{
	_deviceInfo.renderID = RENDERID_OPENGL_AUTO;
	_deviceInfo.renderName = "OpenGL";
	_deviceInfo.isTexturingSupported = true;
	_deviceInfo.isEdgeMarkSupported = true;
	_deviceInfo.isFogSupported = true;
	_deviceInfo.isTextureSmoothingSupported = true;
	_deviceInfo.maxAnisotropy = 1.0f;
	_deviceInfo.maxSamples = 0;
	
	_internalRenderingFormat = NDSColorFormat_BGR888_Rev;
	
	versionMajor = 0;
	versionMinor = 0;
	versionRevision = 0;
	
	isVBOSupported = false;
	isPBOSupported = false;
	isFBOSupported = false;
	isMultisampledFBOSupported = false;
	isShaderSupported = false;
	isSampleShadingSupported = false;
	isVAOSupported = false;
	willFlipOnlyFramebufferOnGPU = false;
	willFlipAndConvertFramebufferOnGPU = false;
	willUsePerSampleZeroDstPass = false;
	
	// Init OpenGL rendering states
	ref = new OGLRenderRef;
	memset(ref, 0, sizeof(OGLRenderRef));
	
	_mappedFramebuffer = NULL;
	_workingTextureUnpackBuffer = (FragmentColor *)malloc_alignedCacheLine(1024 * 1024 * sizeof(FragmentColor));
	_pixelReadNeedsFinish = false;
	_needsZeroDstAlphaPass = true;
	_currentPolyIndex = 0;
	_lastTextureDrawTarget = OGLTextureUnitID_GColor;
}

OpenGLRenderer::~OpenGLRenderer()
{
	free_aligned(_framebufferColor);
	free_aligned(_workingTextureUnpackBuffer);
	
	// Destroy OpenGL rendering states
	delete ref;
	ref = NULL;
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

Render3DError OpenGLRenderer::_FlushFramebufferFlipAndConvertOnCPU(const FragmentColor *__restrict srcFramebuffer,
																   FragmentColor *__restrict dstFramebufferMain, u16 *__restrict dstFramebuffer16,
																   bool doFramebufferFlip, bool doFramebufferConvert)
{
	if ( ((dstFramebufferMain == NULL) && (dstFramebuffer16 == NULL)) || (srcFramebuffer == NULL) )
	{
		return RENDER3DERROR_NOERR;
	}
	
	// Convert from 32-bit BGRA8888 format to 32-bit RGBA6665 reversed format. OpenGL
	// stores pixels using a flipped Y-coordinate, so this needs to be flipped back
	// to the DS Y-coordinate.
	
	size_t i = 0;
	
	if (!doFramebufferFlip)
	{
		const size_t pixCount = this->_framebufferWidth * this->_framebufferHeight;
		
		if (!doFramebufferConvert)
		{
			if ( (dstFramebufferMain != NULL) && (dstFramebuffer16 != NULL) )
			{
#ifdef ENABLE_SSE2
				const size_t ssePixCount = pixCount - (pixCount % 8);
				for (; i < ssePixCount; i += 8)
				{
					const __m128i srcColorLo = _mm_load_si128((__m128i *)(srcFramebuffer + i + 0));
					const __m128i srcColorHi = _mm_load_si128((__m128i *)(srcFramebuffer + i + 4));
					
					_mm_store_si128((__m128i *)(dstFramebufferMain + i + 0), ColorspaceCopy32_SSE2<false>(srcColorLo));
					_mm_store_si128((__m128i *)(dstFramebufferMain + i + 4), ColorspaceCopy32_SSE2<false>(srcColorHi));
					_mm_store_si128( (__m128i *)(dstFramebuffer16 + i), ColorspaceConvert8888To5551_SSE2<false>(srcColorLo, srcColorHi) );
				}
				
#pragma LOOPVECTORIZE_DISABLE
#endif
				for (; i < pixCount; i++)
				{
					dstFramebufferMain[i].color = ColorspaceCopy32<false>(srcFramebuffer[i]);
					dstFramebuffer16[i]         = ColorspaceConvert8888To5551<false>(srcFramebuffer[i]);
				}
				
				this->_renderNeedsFlushMain = false;
				this->_renderNeedsFlush16 = false;
			}
			else if (dstFramebufferMain != NULL)
			{
				ColorspaceCopyBuffer32<false, false>((u32 *)srcFramebuffer, (u32 *)dstFramebufferMain, pixCount);
				this->_renderNeedsFlushMain = false;
			}
			else
			{
				ColorspaceConvertBuffer8888To5551<false, false>((u32 *)srcFramebuffer, dstFramebuffer16, pixCount);
				this->_renderNeedsFlush16 = false;
			}
		}
		else
		{
			if (this->_outputFormat == NDSColorFormat_BGR666_Rev)
			{
				if ( (dstFramebufferMain != NULL) && (dstFramebuffer16 != NULL) )
				{
#ifdef ENABLE_SSE2
					const size_t ssePixCount = pixCount - (pixCount % 8);
					for (; i < ssePixCount; i += 8)
					{
						const __m128i srcColorLo = _mm_load_si128((__m128i *)(srcFramebuffer + i + 0));
						const __m128i srcColorHi = _mm_load_si128((__m128i *)(srcFramebuffer + i + 4));
						
						_mm_store_si128( (__m128i *)(dstFramebufferMain + i + 0), ColorspaceConvert8888To6665_SSE2<true>(srcColorLo) );
						_mm_store_si128( (__m128i *)(dstFramebufferMain + i + 4), ColorspaceConvert8888To6665_SSE2<true>(srcColorHi) );
						_mm_store_si128( (__m128i *)(dstFramebuffer16 + i), ColorspaceConvert8888To5551_SSE2<true>(srcColorLo, srcColorHi) );
					}
					
#pragma LOOPVECTORIZE_DISABLE
#endif
					for (; i < pixCount; i++)
					{
						dstFramebufferMain[i].color = ColorspaceConvert8888To6665<true>(srcFramebuffer[i]);
						dstFramebuffer16[i]         = ColorspaceConvert8888To5551<true>(srcFramebuffer[i]);
					}
					
					this->_renderNeedsFlushMain = false;
					this->_renderNeedsFlush16 = false;
				}
				else if (dstFramebufferMain != NULL)
				{
					ColorspaceConvertBuffer8888To6665<true, false>((u32 *)srcFramebuffer, (u32 *)dstFramebufferMain, pixCount);
					this->_renderNeedsFlushMain = false;
				}
				else
				{
					ColorspaceConvertBuffer8888To5551<true, false>((u32 *)srcFramebuffer, dstFramebuffer16, pixCount);
					this->_renderNeedsFlush16 = false;
				}
			}
			else if (this->_outputFormat == NDSColorFormat_BGR888_Rev)
			{
				if ( (dstFramebufferMain != NULL) && (dstFramebuffer16 != NULL) )
				{
#ifdef ENABLE_SSE2
					const size_t ssePixCount = pixCount - (pixCount % 8);
					for (; i < ssePixCount; i += 8)
					{
						const __m128i srcColorLo = _mm_load_si128((__m128i *)(srcFramebuffer + i + 0));
						const __m128i srcColorHi = _mm_load_si128((__m128i *)(srcFramebuffer + i + 4));
						
						_mm_store_si128((__m128i *)(dstFramebufferMain + i + 0), ColorspaceCopy32_SSE2<true>(srcColorLo));
						_mm_store_si128((__m128i *)(dstFramebufferMain + i + 4), ColorspaceCopy32_SSE2<true>(srcColorHi));
						_mm_store_si128( (__m128i *)(dstFramebuffer16 + i), ColorspaceConvert8888To5551_SSE2<true>(srcColorLo, srcColorHi) );
					}
					
#pragma LOOPVECTORIZE_DISABLE
#endif
					for (; i < pixCount; i++)
					{
						dstFramebufferMain[i].color = ColorspaceCopy32<true>(srcFramebuffer[i]);
						dstFramebuffer16[i]         = ColorspaceConvert8888To5551<true>(srcFramebuffer[i]);
					}
					
					this->_renderNeedsFlushMain = false;
					this->_renderNeedsFlush16 = false;
				}
				else if (dstFramebufferMain != NULL)
				{
					ColorspaceCopyBuffer32<true, false>((u32 *)srcFramebuffer, (u32 *)dstFramebufferMain, pixCount);
					this->_renderNeedsFlushMain = false;
				}
				else
				{
					ColorspaceConvertBuffer8888To5551<true, false>((u32 *)srcFramebuffer, dstFramebuffer16, pixCount);
					this->_renderNeedsFlush16 = false;
				}
			}
		}
	}
	else // In the case where OpenGL couldn't flip the framebuffer on the GPU, we'll instead need to flip the framebuffer during conversion.
	{
		const size_t pixCount = this->_framebufferWidth;
		
		if (!doFramebufferConvert)
		{
			if ( (dstFramebufferMain != NULL) && (dstFramebuffer16 != NULL) )
			{
				for (size_t y = 0, ir = 0, iw = ((this->_framebufferHeight - 1) * this->_framebufferWidth); y < this->_framebufferHeight; y++, ir += this->_framebufferWidth, iw -= this->_framebufferWidth)
				{
					size_t x = 0;
#ifdef ENABLE_SSE2
					const size_t ssePixCount = pixCount - (pixCount % 8);
					for (; x < ssePixCount; x += 8, ir += 8, iw += 8)
					{
						const __m128i srcColorLo = _mm_load_si128((__m128i *)(srcFramebuffer + ir + 0));
						const __m128i srcColorHi = _mm_load_si128((__m128i *)(srcFramebuffer + ir + 4));
						
						_mm_store_si128( (__m128i *)(dstFramebufferMain + iw + 0), ColorspaceCopy32_SSE2<false>(srcColorLo) );
						_mm_store_si128( (__m128i *)(dstFramebufferMain + iw + 4), ColorspaceCopy32_SSE2<false>(srcColorHi) );
						_mm_store_si128( (__m128i *)(dstFramebuffer16 + iw), ColorspaceConvert8888To5551_SSE2<false>(srcColorLo, srcColorHi) );
					}
					
#pragma LOOPVECTORIZE_DISABLE
#endif
					for (; x < pixCount; x++, ir++, iw++)
					{
						dstFramebufferMain[iw].color = ColorspaceCopy32<false>(srcFramebuffer[ir]);
						dstFramebuffer16[iw]         = ColorspaceConvert8888To5551<false>(srcFramebuffer[ir]);
					}
				}
				
				this->_renderNeedsFlushMain = false;
				this->_renderNeedsFlush16 = false;
			}
			else if (dstFramebufferMain != NULL)
			{
				for (size_t y = 0, ir = 0, iw = ((this->_framebufferHeight - 1) * this->_framebufferWidth); y < this->_framebufferHeight; y++, ir += this->_framebufferWidth, iw -= this->_framebufferWidth)
				{
					ColorspaceCopyBuffer32<false, false>((u32 *)srcFramebuffer + ir, (u32 *)dstFramebufferMain + iw, pixCount);
				}
				
				this->_renderNeedsFlushMain = false;
			}
			else
			{
				for (size_t y = 0, ir = 0, iw = ((this->_framebufferHeight - 1) * this->_framebufferWidth); y < this->_framebufferHeight; y++, ir += this->_framebufferWidth, iw -= this->_framebufferWidth)
				{
					ColorspaceConvertBuffer8888To5551<false, false>((u32 *)srcFramebuffer + ir, dstFramebuffer16 + iw, pixCount);
				}
				
				this->_renderNeedsFlush16 = false;
			}
		}
		else
		{
			if (this->_outputFormat == NDSColorFormat_BGR666_Rev)
			{
				if ( (dstFramebufferMain != NULL) && (dstFramebuffer16 != NULL) )
				{
					for (size_t y = 0, ir = 0, iw = ((this->_framebufferHeight - 1) * this->_framebufferWidth); y < this->_framebufferHeight; y++, ir += this->_framebufferWidth, iw -= this->_framebufferWidth)
					{
						size_t x = 0;
#ifdef ENABLE_SSE2
						const size_t ssePixCount = pixCount - (pixCount % 8);
						for (; x < ssePixCount; x += 8, ir += 8, iw += 8)
						{
							const __m128i srcColorLo = _mm_load_si128((__m128i *)(srcFramebuffer + ir + 0));
							const __m128i srcColorHi = _mm_load_si128((__m128i *)(srcFramebuffer + ir + 4));
							
							_mm_store_si128( (__m128i *)(dstFramebufferMain + iw + 0), ColorspaceConvert8888To6665_SSE2<true>(srcColorLo) );
							_mm_store_si128( (__m128i *)(dstFramebufferMain + iw + 4), ColorspaceConvert8888To6665_SSE2<true>(srcColorHi) );
							_mm_store_si128( (__m128i *)(dstFramebuffer16 + iw), ColorspaceConvert8888To5551_SSE2<true>(srcColorLo, srcColorHi) );
						}
						
#pragma LOOPVECTORIZE_DISABLE
#endif
						for (; x < pixCount; x++, ir++, iw++)
						{
							dstFramebufferMain[iw].color = ColorspaceConvert8888To6665<true>(srcFramebuffer[ir]);
							dstFramebuffer16[iw]         = ColorspaceConvert8888To5551<true>(srcFramebuffer[ir]);
						}
					}
					
					this->_renderNeedsFlushMain = false;
					this->_renderNeedsFlush16 = false;
				}
				else if (dstFramebufferMain != NULL)
				{
					for (size_t y = 0, ir = 0, iw = ((this->_framebufferHeight - 1) * this->_framebufferWidth); y < this->_framebufferHeight; y++, ir += this->_framebufferWidth, iw -= this->_framebufferWidth)
					{
						ColorspaceConvertBuffer8888To6665<true, false>((u32 *)srcFramebuffer + ir, (u32 *)dstFramebufferMain + iw, pixCount);
					}
					
					this->_renderNeedsFlushMain = false;
				}
				else
				{
					for (size_t y = 0, ir = 0, iw = ((this->_framebufferHeight - 1) * this->_framebufferWidth); y < this->_framebufferHeight; y++, ir += this->_framebufferWidth, iw -= this->_framebufferWidth)
					{
						ColorspaceConvertBuffer8888To5551<true, false>((u32 *)srcFramebuffer + ir, dstFramebuffer16 + iw, pixCount);
					}
					
					this->_renderNeedsFlush16 = false;
				}
			}
			else if (this->_outputFormat == NDSColorFormat_BGR888_Rev)
			{
				if ( (dstFramebufferMain != NULL) && (dstFramebuffer16 != NULL) )
				{
					for (size_t y = 0, ir = 0, iw = ((this->_framebufferHeight - 1) * this->_framebufferWidth); y < this->_framebufferHeight; y++, ir += this->_framebufferWidth, iw -= this->_framebufferWidth)
					{
						size_t x = 0;
#ifdef ENABLE_SSE2
						const size_t ssePixCount = pixCount - (pixCount % 8);
						for (; x < ssePixCount; x += 8, ir += 8, iw += 8)
						{
							const __m128i srcColorLo = _mm_load_si128((__m128i *)(srcFramebuffer + ir + 0));
							const __m128i srcColorHi = _mm_load_si128((__m128i *)(srcFramebuffer + ir + 4));
							
							_mm_store_si128((__m128i *)(dstFramebufferMain + iw + 0), ColorspaceCopy32_SSE2<true>(srcColorLo));
							_mm_store_si128((__m128i *)(dstFramebufferMain + iw + 4), ColorspaceCopy32_SSE2<true>(srcColorHi));
							_mm_store_si128( (__m128i *)(dstFramebuffer16 + iw), ColorspaceConvert8888To5551_SSE2<true>(srcColorLo, srcColorHi) );
						}
						
#pragma LOOPVECTORIZE_DISABLE
#endif
						for (; x < pixCount; x++, ir++, iw++)
						{
							dstFramebufferMain[iw].color = ColorspaceCopy32<true>(srcFramebuffer[ir]);
							dstFramebuffer16[iw]         = ColorspaceConvert8888To5551<true>(srcFramebuffer[ir]);
						}
					}
					
					this->_renderNeedsFlushMain = false;
					this->_renderNeedsFlush16 = false;
				}
				else if (dstFramebufferMain != NULL)
				{
					for (size_t y = 0, ir = 0, iw = ((this->_framebufferHeight - 1) * this->_framebufferWidth); y < this->_framebufferHeight; y++, ir += this->_framebufferWidth, iw -= this->_framebufferWidth)
					{
						ColorspaceCopyBuffer32<true, false>((u32 *)srcFramebuffer + ir, (u32 *)dstFramebufferMain + iw, pixCount);
					}
					
					this->_renderNeedsFlushMain = false;
				}
				else
				{
					for (size_t y = 0, ir = 0, iw = ((this->_framebufferHeight - 1) * this->_framebufferWidth); y < this->_framebufferHeight; y++, ir += this->_framebufferWidth, iw -= this->_framebufferWidth)
					{
						ColorspaceConvertBuffer8888To5551<true, false>((u32 *)srcFramebuffer + ir, dstFramebuffer16 + iw, pixCount);
					}
					
					this->_renderNeedsFlush16 = false;
				}
			}
		}
	}
	
	return RENDER3DERROR_NOERR;
}

Render3DError OpenGLRenderer::FlushFramebuffer(const FragmentColor *__restrict srcFramebuffer, FragmentColor *__restrict dstFramebufferMain, u16 *__restrict dstFramebuffer16)
{
	if (this->willFlipAndConvertFramebufferOnGPU && this->isPBOSupported)
	{
		this->_renderNeedsFlushMain = false;
		return Render3D::FlushFramebuffer(srcFramebuffer, NULL, dstFramebuffer16);
	}
	else
	{
		return this->_FlushFramebufferFlipAndConvertOnCPU(srcFramebuffer,
														  dstFramebufferMain, dstFramebuffer16,
														  !this->willFlipOnlyFramebufferOnGPU, !this->willFlipAndConvertFramebufferOnGPU);
	}
	
	return RENDER3DERROR_NOERR;
}

FragmentColor* OpenGLRenderer::GetFramebuffer()
{
	return (this->willFlipAndConvertFramebufferOnGPU && this->isPBOSupported) ? this->_mappedFramebuffer : GPU->GetEngineMain()->Get3DFramebufferMain();
}


GLsizei OpenGLRenderer::GetLimitedMultisampleSize() const
{
	GLsizei deviceMultisamples = this->_deviceInfo.maxSamples;
	GLsizei maxMultisamples = OGLMaxMultisamples_Tier1;
	
	if ( (this->_framebufferWidth  <= GPU_FRAMEBUFFER_NATIVE_WIDTH  * OGLMaxMultisamplesScaleLimit_Tier1) &&
		 (this->_framebufferHeight <= GPU_FRAMEBUFFER_NATIVE_HEIGHT * OGLMaxMultisamplesScaleLimit_Tier1) )
	{
		maxMultisamples = OGLMaxMultisamples_Tier1;
	}
	else if ( (this->_framebufferWidth  <= GPU_FRAMEBUFFER_NATIVE_WIDTH  * OGLMaxMultisamplesScaleLimit_Tier2) &&
			  (this->_framebufferHeight <= GPU_FRAMEBUFFER_NATIVE_HEIGHT * OGLMaxMultisamplesScaleLimit_Tier2) )
	{
		maxMultisamples = OGLMaxMultisamples_Tier2;
	}
	else if ( (this->_framebufferWidth  <= GPU_FRAMEBUFFER_NATIVE_WIDTH  * OGLMaxMultisamplesScaleLimit_Tier3) &&
			  (this->_framebufferHeight <= GPU_FRAMEBUFFER_NATIVE_HEIGHT * OGLMaxMultisamplesScaleLimit_Tier3) )
	{
		maxMultisamples = OGLMaxMultisamples_Tier3;
	}
	else
	{
		maxMultisamples = OGLMaxMultisamples_Tier4;
	}
	
	if (deviceMultisamples > maxMultisamples)
	{
		deviceMultisamples = maxMultisamples;
	}
	
	return deviceMultisamples;
}

OpenGLTexture* OpenGLRenderer::GetLoadedTextureFromPolygon(const POLY &thePoly, bool enableTexturing)
{
	OpenGLTexture *theTexture = (OpenGLTexture *)texCache.GetTexture(thePoly.texParam, thePoly.texPalette);
	const bool isNewTexture = (theTexture == NULL);
	
	if (isNewTexture)
	{
		theTexture = new OpenGLTexture(thePoly.texParam, thePoly.texPalette);
		theTexture->SetUnpackBuffer(this->_workingTextureUnpackBuffer);
		
		texCache.Add(theTexture);
	}
	
	const NDSTextureFormat packFormat = theTexture->GetPackFormat();
	const bool isTextureEnabled = ( (packFormat != TEXMODE_NONE) && enableTexturing );
	
	theTexture->SetSamplingEnabled(isTextureEnabled);
	
	if (theTexture->IsLoadNeeded() && isTextureEnabled)
	{
		const size_t previousScalingFactor = theTexture->GetScalingFactor();
		
		theTexture->SetDeposterizeBuffer(this->_workingTextureUnpackBuffer, this->_textureDeposterizeDstSurface.workingSurface[0]);
		theTexture->SetUpscalingBuffer(this->_textureUpscaleBuffer);
		
		theTexture->SetUseDeposterize(this->_enableTextureDeposterize);
		theTexture->SetScalingFactor(this->_textureScalingFactor);
		
		theTexture->Load(isNewTexture || (previousScalingFactor != this->_textureScalingFactor));
	}
	
	return theTexture;
}

template <OGLPolyDrawMode DRAWMODE>
size_t OpenGLRenderer::DrawPolygonsForIndexRange(const POLYLIST *polyList, const INDEXLIST *indexList, size_t firstIndex, size_t lastIndex, size_t &indexOffset, POLYGON_ATTR &lastPolyAttr)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (lastIndex > (polyList->count - 1))
	{
		lastIndex = polyList->count - 1;
	}
	
	if (firstIndex > lastIndex)
	{
		return 0;
	}
	
	// Map GFX3D_QUADS and GFX3D_QUAD_STRIP to GL_TRIANGLES since we will convert them.
	//
	// Also map GFX3D_TRIANGLE_STRIP to GL_TRIANGLES. This is okay since this is actually
	// how the POLY struct stores triangle strip vertices, which is in sets of 3 vertices
	// each. This redefinition is necessary since uploading more than 3 indices at a time
	// will cause glDrawElements() to draw the triangle strip incorrectly.
	static const GLenum oglPrimitiveType[]	= { GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES,
	                                            GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_STRIP, GL_LINE_STRIP };
	
	static const GLsizei indexIncrementLUT[] = {3, 6, 3, 6, 3, 4, 3, 4};
	
	// Set up the initial polygon
	const POLY &initialPoly = polyList->list[indexList->list[firstIndex]];
	TEXIMAGE_PARAM lastTexParams = initialPoly.texParam;
	u32 lastTexPalette = initialPoly.texPalette;
	u32 lastViewport = initialPoly.viewport;
	
	this->SetupTexture(initialPoly, firstIndex);
	this->SetupViewport(initialPoly.viewport);
	
	// Enumerate through all polygons and render
	GLsizei vertIndexCount = 0;
	GLushort *indexBufferPtr = OGLRef.vertIndexBuffer + indexOffset;
	
	for (size_t i = firstIndex; i <= lastIndex; i++)
	{
		const POLY &thePoly = polyList->list[indexList->list[i]];
		
		// Set up the polygon if it changed
		if (lastPolyAttr.value != thePoly.attribute.value)
		{
			lastPolyAttr = thePoly.attribute;
			this->SetupPolygon(thePoly, (DRAWMODE != OGLPolyDrawMode_DrawOpaquePolys), (DRAWMODE != OGLPolyDrawMode_ZeroAlphaPass));
		}
		
		// Set up the texture if it changed
		if (lastTexParams.value != thePoly.texParam.value || lastTexPalette != thePoly.texPalette)
		{
			lastTexParams = thePoly.texParam;
			lastTexPalette = thePoly.texPalette;
			this->SetupTexture(thePoly, i);
		}
		
		// Set up the viewport if it changed
		if (lastViewport != thePoly.viewport)
		{
			lastViewport = thePoly.viewport;
			this->SetupViewport(thePoly.viewport);
		}
		
		// In wireframe mode, redefine all primitives as GL_LINE_LOOP rather than
		// setting the polygon mode to GL_LINE though glPolygonMode(). Not only is
		// drawing more accurate this way, but it also allows GFX3D_QUADS and
		// GFX3D_QUAD_STRIP primitives to properly draw as wireframe without the
		// extra diagonal line.
		const GLenum polyPrimitive = (!thePoly.isWireframe()) ? oglPrimitiveType[thePoly.vtxFormat] : GL_LINE_LOOP;
		
		// Increment the vertex count
		vertIndexCount += indexIncrementLUT[thePoly.vtxFormat];
		
		// Look ahead to the next polygon to see if we can simply buffer the indices
		// instead of uploading them now. We can buffer if all polygon states remain
		// the same and we're not drawing a line loop or line strip.
		if (i+1 <= lastIndex)
		{
			const POLY &nextPoly = polyList->list[indexList->list[i+1]];
			
			if (lastPolyAttr.value == nextPoly.attribute.value &&
				lastTexParams.value == nextPoly.texParam.value &&
				lastTexPalette == nextPoly.texPalette &&
				lastViewport == nextPoly.viewport &&
				polyPrimitive == oglPrimitiveType[nextPoly.vtxFormat] &&
				polyPrimitive != GL_LINE_LOOP &&
				polyPrimitive != GL_LINE_STRIP &&
				oglPrimitiveType[nextPoly.vtxFormat] != GL_LINE_LOOP &&
				oglPrimitiveType[nextPoly.vtxFormat] != GL_LINE_STRIP)
			{
				continue;
			}
		}
		
		// Render the polygons
		this->SetPolygonIndex(i);
		
		if (thePoly.attribute.Mode == POLYGON_MODE_SHADOW)
		{
			if (DRAWMODE != OGLPolyDrawMode_ZeroAlphaPass)
			{
				this->DrawShadowPolygon(polyPrimitive, vertIndexCount, indexBufferPtr, thePoly.attribute.DepthEqualTest_Enable, thePoly.attribute.TranslucentDepthWrite_Enable, (DRAWMODE == OGLPolyDrawMode_DrawTranslucentPolys), thePoly.attribute.PolygonID);
			}
		}
		else if ( (thePoly.texParam.PackedFormat == TEXMODE_A3I5) || (thePoly.texParam.PackedFormat == TEXMODE_A5I3) )
		{
			this->DrawAlphaTexturePolygon<DRAWMODE>(polyPrimitive, vertIndexCount, indexBufferPtr, thePoly.attribute.DepthEqualTest_Enable, thePoly.attribute.TranslucentDepthWrite_Enable, thePoly.isWireframe() || thePoly.isOpaque(), thePoly.attribute.PolygonID);
		}
		else
		{
			this->DrawOtherPolygon<DRAWMODE>(polyPrimitive, vertIndexCount, indexBufferPtr, thePoly.attribute.DepthEqualTest_Enable, thePoly.attribute.TranslucentDepthWrite_Enable, thePoly.attribute.PolygonID);
		}
		
		indexBufferPtr += vertIndexCount;
		indexOffset += vertIndexCount;
		vertIndexCount = 0;
	}
	
	return indexOffset;
}

template <OGLPolyDrawMode DRAWMODE>
Render3DError OpenGLRenderer::DrawAlphaTexturePolygon(const GLenum polyPrimitive, const GLsizei vertIndexCount, const GLushort *indexBufferPtr, const bool performDepthEqualTest, const bool enableAlphaDepthWrite, const bool canHaveOpaqueFragments, const u8 opaquePolyID)
{
	const OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isShaderSupported)
	{
		if ((DRAWMODE != OGLPolyDrawMode_ZeroAlphaPass) && performDepthEqualTest)
		{
			if (DRAWMODE == OGLPolyDrawMode_DrawTranslucentPolys)
			{
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
				glDepthMask(GL_FALSE);
				
				// Use the stencil buffer to determine which fragments pass the lower-side tolerance.
				glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 1);
				glDepthFunc(GL_LEQUAL);
				glStencilFunc(GL_ALWAYS, 0x80, 0x80);
				glStencilOp(GL_ZERO, GL_ZERO, GL_REPLACE);
				glStencilMask(0x80);
				
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				if (canHaveOpaqueFragments)
				{
					glUniform1i(OGLRef.uniformTexDrawOpaque, GL_TRUE);
					glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
					glUniform1i(OGLRef.uniformTexDrawOpaque, GL_FALSE);
				}
				
				// Use the stencil buffer to determine which fragments pass the higher-side tolerance.
				glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 2);
				glDepthFunc(GL_GEQUAL);
				glStencilFunc(GL_EQUAL, 0x80, 0x80);
				glStencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
				glStencilMask(0x80);
				
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				if (canHaveOpaqueFragments)
				{
					glUniform1i(OGLRef.uniformTexDrawOpaque, GL_TRUE);
					glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
					glUniform1i(OGLRef.uniformTexDrawOpaque, GL_FALSE);
				}
				
				// Set up the actual drawing of the polygon, using the mask within the stencil buffer to determine which fragments should pass.
				glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 0);
				glDepthFunc(GL_ALWAYS);
				
				// First do the transparent polygon ID check for the translucent fragments.
				glStencilFunc(GL_NOTEQUAL, 0x40 | opaquePolyID, 0x7F);
				glStencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
				glStencilMask(0x80);
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				// Draw the translucent fragments.
				glStencilFunc(GL_EQUAL, 0xC0 | opaquePolyID, 0x80);
				glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
				glStencilMask(0x7F);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glDepthMask((enableAlphaDepthWrite) ? GL_TRUE : GL_FALSE);
				
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				// Draw the opaque fragments if they might exist.
				if (canHaveOpaqueFragments)
				{
					glStencilFunc(GL_EQUAL, 0x80 | opaquePolyID, 0x80);
					glDepthMask(GL_TRUE);
					glUniform1i(OGLRef.uniformTexDrawOpaque, GL_TRUE);
					glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
					glUniform1i(OGLRef.uniformTexDrawOpaque, GL_FALSE);
				}
				
				// Clear bit 7 (0x80) now so that future polygons don't get confused.
				glStencilFunc(GL_ALWAYS, 0x80, 0x80);
				glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
				glStencilMask(0x80);
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
				glDepthMask(GL_FALSE);
				
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				if (canHaveOpaqueFragments)
				{
					glUniform1i(OGLRef.uniformTexDrawOpaque, GL_TRUE);
					glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
					glUniform1i(OGLRef.uniformTexDrawOpaque, GL_FALSE);
				}
				
				// Finally, reset the rendering states.
				glStencilFunc(GL_NOTEQUAL, 0x40 | opaquePolyID, 0x7F);
				glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
				glStencilMask(0xFF);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glDepthMask((enableAlphaDepthWrite) ? GL_TRUE : GL_FALSE);
			}
			else
			{
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
				glDepthMask(GL_FALSE);
				
				glUniform1i(OGLRef.uniformTexDrawOpaque, GL_TRUE);
				
				// Use the stencil buffer to determine which fragments pass the lower-side tolerance.
				glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 1);
				glDepthFunc(GL_LEQUAL);
				glStencilFunc(GL_ALWAYS, 0x80, 0x80);
				glStencilOp(GL_ZERO, GL_ZERO, GL_REPLACE);
				glStencilMask(0x80);
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				// Use the stencil buffer to determine which fragments pass the higher-side tolerance.
				glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 2);
				glDepthFunc(GL_GEQUAL);
				glStencilFunc(GL_EQUAL, 0x80, 0x80);
				glStencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
				glStencilMask(0x80);
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				// Set up the actual drawing of the polygon, using the mask within the stencil buffer to determine which fragments should pass.
				glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 0);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glDepthMask(GL_TRUE);
				glDepthFunc(GL_ALWAYS);
				glStencilFunc(GL_EQUAL, 0x80 | opaquePolyID, 0x80);
				glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
				glStencilMask(0x7F);
				
				// Draw the polygon as completely opaque.
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				// Clear bit 7 (0x80) now so that future polygons don't get confused.
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
				glDepthMask(GL_FALSE);
				
				glStencilFunc(GL_ALWAYS, 0x80, 0x80);
				glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
				glStencilMask(0x80);
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				// Finally, reset the rendering states.
				glStencilFunc(GL_ALWAYS, opaquePolyID, 0x3F);
				glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
				glStencilMask(0xFF);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glDepthMask(GL_TRUE);
				
				glUniform1i(OGLRef.uniformTexDrawOpaque, GL_FALSE);
			}
		}
		else
		{
			if (DRAWMODE != OGLPolyDrawMode_DrawOpaquePolys)
			{
				// Draw the translucent fragments.
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				// Draw the opaque fragments if they might exist.
				if (canHaveOpaqueFragments)
				{
					if (DRAWMODE != OGLPolyDrawMode_ZeroAlphaPass)
					{
						glStencilFunc(GL_ALWAYS, opaquePolyID, 0x3F);
						glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
						glDepthMask(GL_TRUE);
					}
					
					glUniform1i(OGLRef.uniformTexDrawOpaque, GL_TRUE);
					glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
					glUniform1i(OGLRef.uniformTexDrawOpaque, GL_FALSE);
					
					if (DRAWMODE != OGLPolyDrawMode_ZeroAlphaPass)
					{
						glStencilFunc(GL_NOTEQUAL, 0x40 | opaquePolyID, 0x7F);
						glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
						glDepthMask((enableAlphaDepthWrite) ? GL_TRUE : GL_FALSE);
					}
				}
			}
			else
			{
				// Draw the polygon as completely opaque.
				glUniform1i(OGLRef.uniformTexDrawOpaque, GL_TRUE);
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				glUniform1i(OGLRef.uniformTexDrawOpaque, GL_FALSE);
			}
		}
	}
	else
	{
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
	}
	
	return OGLERROR_NOERR;
}

template <OGLPolyDrawMode DRAWMODE>
Render3DError OpenGLRenderer::DrawOtherPolygon(const GLenum polyPrimitive, const GLsizei vertIndexCount, const GLushort *indexBufferPtr, const bool performDepthEqualTest, const bool enableAlphaDepthWrite, const u8 opaquePolyID)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if ((DRAWMODE != OGLPolyDrawMode_ZeroAlphaPass) && performDepthEqualTest && this->isShaderSupported)
	{
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_FALSE);
		
		// Use the stencil buffer to determine which fragments pass the lower-side tolerance.
		glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 1);
		glDepthFunc(GL_LEQUAL);
		glStencilFunc(GL_ALWAYS, 0x80, 0x80);
		glStencilOp(GL_ZERO, GL_ZERO, GL_REPLACE);
		glStencilMask(0x80);
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		
		// Use the stencil buffer to determine which fragments pass the higher-side tolerance.
		glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 2);
		glDepthFunc(GL_GEQUAL);
		glStencilFunc(GL_EQUAL, 0x80, 0x80);
		glStencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
		glStencilMask(0x80);
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		
		// Set up the actual drawing of the polygon.
		glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 0);
		glDepthFunc(GL_ALWAYS);
		
		// If this is a transparent polygon, then we need to do the transparent polygon ID check.
		if (DRAWMODE == OGLPolyDrawMode_DrawTranslucentPolys)
		{
			glStencilFunc(GL_NOTEQUAL, 0x40 | opaquePolyID, 0x7F);
			glStencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
			glStencilMask(0x80);
			glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		}
		
		// Draw the polygon using the mask within the stencil buffer to determine which fragments should pass.
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(((DRAWMODE == OGLPolyDrawMode_DrawOpaquePolys) || enableAlphaDepthWrite) ? GL_TRUE : GL_FALSE);
		
		glStencilFunc(GL_EQUAL, (DRAWMODE == OGLPolyDrawMode_DrawTranslucentPolys) ? 0xC0 | opaquePolyID : 0x80 | opaquePolyID, 0x80);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilMask(0x7F);
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		
		// Clear bit 7 (0x80) now so that future polygons don't get confused.
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_FALSE);
		
		glStencilFunc(GL_ALWAYS, 0x80, 0x80);
		glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
		glStencilMask(0x80);
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		
		// Finally, reset the rendering states.
		if (DRAWMODE == OGLPolyDrawMode_DrawTranslucentPolys)
		{
			glStencilFunc(GL_NOTEQUAL, 0x40 | opaquePolyID, 0x7F);
		}
		else
		{
			glStencilFunc(GL_ALWAYS, opaquePolyID, 0x3F);
		}
		
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilMask(0xFF);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(((DRAWMODE == OGLPolyDrawMode_DrawOpaquePolys) || enableAlphaDepthWrite) ? GL_TRUE : GL_FALSE);
	}
	else
	{
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer::ApplyRenderingSettings(const GFX3D_State &renderState)
{
	this->_enableMultisampledRendering = (CommonSettings.GFX3D_Renderer_Multisample && this->isMultisampledFBOSupported);
	
	return Render3D::ApplyRenderingSettings(renderState);
}

OpenGLRenderer_1_2::~OpenGLRenderer_1_2()
{
	glFinish();
	
	_pixelReadNeedsFinish = false;
	
	delete[] ref->color4fBuffer;
	ref->color4fBuffer = NULL;
	
	delete[] ref->vertIndexBuffer;
	ref->vertIndexBuffer = NULL;
	
	DestroyPostprocessingPrograms();
	DestroyGeometryProgram();
	isShaderSupported = false;
	
	DestroyVAOs();
	DestroyVBOs();
	DestroyPBOs();
	DestroyFBOs();
	DestroyMultisampledFBO();
	
	// Kill the texture cache now before all of our texture IDs disappear.
	texCache.Reset();
	
	glDeleteTextures(1, &ref->texFinalColorID);
	ref->texFinalColorID = 0;
	
	glFinish();
}

Render3DError OpenGLRenderer_1_2::InitExtensions()
{
	OGLRenderRef &OGLRef = *this->ref;
	Render3DError error = OGLERROR_NOERR;
	
	// Get OpenGL extensions
	std::set<std::string> oglExtensionSet;
	this->GetExtensionSet(&oglExtensionSet);
	
	if (!this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_multitexture"))
	{
		return OGLERROR_DRIVER_VERSION_TOO_OLD;
	}
	else
	{
		GLint maxFixedFunctionTexUnitsOGL = 0;
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &maxFixedFunctionTexUnitsOGL);
		
		if (maxFixedFunctionTexUnitsOGL < 4)
		{
			return OGLERROR_DRIVER_VERSION_TOO_OLD;
		}
	}
	
	// Get host GPU device properties
	GLfloat maxAnisotropyOGL = 1.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropyOGL);
	this->_deviceInfo.maxAnisotropy = maxAnisotropyOGL;
	
	// Initialize OpenGL
	this->InitTables();
	
	this->isShaderSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_shader_objects") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_shader") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_fragment_shader") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_program");
	if (this->isShaderSupported)
	{
		GLint maxDrawBuffersOGL = 0;
		GLint maxShaderTexUnitsOGL = 0;
		glGetIntegerv(GL_MAX_DRAW_BUFFERS_ARB, &maxDrawBuffersOGL);
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &maxShaderTexUnitsOGL);
		
		if ( (maxDrawBuffersOGL >= 4) && (maxShaderTexUnitsOGL >= 8) )
		{
			error = this->InitGeometryProgram(GeometryVtxShader_100, GeometryFragShader_100,
											  GeometryZeroDstAlphaPixelMaskVtxShader_100, GeometryZeroDstAlphaPixelMaskFragShader_100,
											  NULL, NULL);
			if (error == OGLERROR_NOERR)
			{
				error = this->InitPostprocessingPrograms(EdgeMarkVtxShader_100,
														 EdgeMarkFragShader_100,
														 FogVtxShader_100,
														 FogFragShader_100,
														 FramebufferOutputVtxShader_100,
														 FramebufferOutputRGBA6665FragShader_100,
														 FramebufferOutputRGBA8888FragShader_100);
			}
			
			if (error != OGLERROR_NOERR)
			{
				this->DestroyPostprocessingPrograms();
				this->DestroyGeometryProgram();
				this->isShaderSupported = false;
			}
		}
		else
		{
			INFO("OpenGL: Driver does not support at least 4 draw buffers and 8 texture image units.\n");
			this->isShaderSupported = false;
		}
	}
	
	if (this->isShaderSupported)
	{
		glGenTextures(1, &OGLRef.texFinalColorID);
		glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_FinalColor);
		glBindTexture(GL_TEXTURE_2D, OGLRef.texFinalColorID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
		glActiveTextureARB(GL_TEXTURE0_ARB);
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
	
	this->isPBOSupported	= this->isVBOSupported &&
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
		GLint maxColorAttachments = 0;
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &maxColorAttachments);
		
		if (maxColorAttachments >= 4)
		{
			// This texture will be used as an FBO color attachment.
			// If this texture wasn't already created by passing the shader support check,
			// then create the texture now.
			bool createdTextureForFBO = false;
			if (OGLRef.texFinalColorID == 0)
			{
				glGenTextures(1, &OGLRef.texFinalColorID);
				glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_FinalColor);
				glBindTexture(GL_TEXTURE_2D, OGLRef.texFinalColorID);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
				glActiveTextureARB(GL_TEXTURE0_ARB);
				
				createdTextureForFBO = true;
			}
			
			error = this->CreateFBOs();
			if (error != OGLERROR_NOERR)
			{
				this->isFBOSupported = false;
				
				if (createdTextureForFBO)
				{
					glDeleteTextures(1, &OGLRef.texFinalColorID);
					OGLRef.texFinalColorID = 0;
				}
			}
		}
		else
		{
			INFO("OpenGL: Driver does not support at least 4 FBO color attachments.\n");
			this->isFBOSupported = false;
		}
	}
	
	if (!this->isFBOSupported)
	{
		INFO("OpenGL: FBOs are unsupported. Some emulation features will be disabled.\n");
	}
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
	this->isMultisampledFBOSupported	= this->isFBOSupported &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_multisample");
	if (this->isMultisampledFBOSupported)
	{
		GLint maxSamplesOGL = 0;
		glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSamplesOGL);
		this->_deviceInfo.maxSamples = (u8)maxSamplesOGL;
		
		if (maxSamplesOGL >= 2)
		{
			GLsizei sampleSize = this->GetLimitedMultisampleSize();
			
			error = this->CreateMultisampledFBO(sampleSize);
			if (error != OGLERROR_NOERR)
			{
				this->isMultisampledFBOSupported = false;
			}
		}
		else
		{
			this->isMultisampledFBOSupported = false;
			INFO("OpenGL: Driver does not support at least 2x multisampled FBOs.\n");
		}
	}
	
	if (!this->isMultisampledFBOSupported)
	{
		INFO("OpenGL: Multisampled FBOs are unsupported. Multisample antialiasing will be disabled.\n");
	}
	
	// Set rendering support flags based on driver features.
	this->willFlipAndConvertFramebufferOnGPU = this->isShaderSupported && this->isVBOSupported;
	this->willFlipOnlyFramebufferOnGPU = this->willFlipAndConvertFramebufferOnGPU || this->isFBOSupported;
	this->_deviceInfo.isEdgeMarkSupported = (this->isShaderSupported && this->isVBOSupported && this->isFBOSupported);
	this->_deviceInfo.isFogSupported = (this->isShaderSupported && this->isVBOSupported && this->isFBOSupported);
	this->_deviceInfo.isTextureSmoothingSupported = this->isShaderSupported;
	
	this->_enableMultisampledRendering = (CommonSettings.GFX3D_Renderer_Multisample && this->isMultisampledFBOSupported);
	
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
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenBuffersARB(1, &OGLRef.pboRenderDataID);
	glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, OGLRef.pboRenderDataID);
	glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, this->_framebufferColorSizeBytes, NULL, GL_STREAM_READ_ARB);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyPBOs()
{
	if (!this->isPBOSupported)
	{
		return;
	}
	
	if (this->_mappedFramebuffer != NULL)
	{
		glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
		this->_mappedFramebuffer = NULL;
	}
	
	glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	glDeleteBuffersARB(1, &this->ref->pboRenderDataID);
	
	this->isPBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::InitGeometryProgramBindings()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindAttribLocation(OGLRef.programGeometryID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programGeometryID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	glBindAttribLocation(OGLRef.programGeometryID, OGLVertexAttributeID_Color, "inColor");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitGeometryProgramShaderLocations()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glUseProgram(OGLRef.programGeometryID);
	
	const GLint uniformTexRenderObject			= glGetUniformLocation(OGLRef.programGeometryID, "texRenderObject");
	const GLint uniformTexToonTable				= glGetUniformLocation(OGLRef.programGeometryID, "texToonTable");
	glUniform1i(uniformTexRenderObject, 0);
	glUniform1i(uniformTexToonTable, OGLTextureUnitID_ToonTable);
	
	OGLRef.uniformStateToonShadingMode			= glGetUniformLocation(OGLRef.programGeometryID, "stateToonShadingMode");
	OGLRef.uniformStateEnableAlphaTest			= glGetUniformLocation(OGLRef.programGeometryID, "stateEnableAlphaTest");
	OGLRef.uniformStateEnableAntialiasing		= glGetUniformLocation(OGLRef.programGeometryID, "stateEnableAntialiasing");
	OGLRef.uniformStateEnableEdgeMarking		= glGetUniformLocation(OGLRef.programGeometryID, "stateEnableEdgeMarking");
	OGLRef.uniformStateUseWDepth				= glGetUniformLocation(OGLRef.programGeometryID, "stateUseWDepth");
	OGLRef.uniformStateAlphaTestRef				= glGetUniformLocation(OGLRef.programGeometryID, "stateAlphaTestRef");
	
	OGLRef.uniformPolyTexScale					= glGetUniformLocation(OGLRef.programGeometryID, "polyTexScale");
	OGLRef.uniformPolyMode						= glGetUniformLocation(OGLRef.programGeometryID, "polyMode");
	OGLRef.uniformPolyIsWireframe				= glGetUniformLocation(OGLRef.programGeometryID, "polyIsWireframe");
	OGLRef.uniformPolySetNewDepthForTranslucent	= glGetUniformLocation(OGLRef.programGeometryID, "polySetNewDepthForTranslucent");
	OGLRef.uniformPolyAlpha						= glGetUniformLocation(OGLRef.programGeometryID, "polyAlpha");
	OGLRef.uniformPolyID						= glGetUniformLocation(OGLRef.programGeometryID, "polyID");
	
	OGLRef.uniformPolyEnableTexture				= glGetUniformLocation(OGLRef.programGeometryID, "polyEnableTexture");
	OGLRef.uniformPolyEnableFog					= glGetUniformLocation(OGLRef.programGeometryID, "polyEnableFog");
	OGLRef.uniformTexSingleBitAlpha				= glGetUniformLocation(OGLRef.programGeometryID, "texSingleBitAlpha");
	
	OGLRef.uniformTexDrawOpaque					= glGetUniformLocation(OGLRef.programGeometryID, "texDrawOpaque");
	OGLRef.uniformPolyDrawShadow				= glGetUniformLocation(OGLRef.programGeometryID, "polyDrawShadow");
	OGLRef.uniformPolyDepthOffsetMode			= glGetUniformLocation(OGLRef.programGeometryID, "polyDepthOffsetMode");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitGeometryZeroDstAlphaProgramBindings()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindAttribLocation(OGLRef.programGeometryZeroDstAlphaID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programGeometryZeroDstAlphaID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitGeometryZeroDstAlphaProgramShaderLocations()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glUseProgram(OGLRef.programGeometryZeroDstAlphaID);
	const GLint uniformTexGColor = glGetUniformLocation(OGLRef.programGeometryZeroDstAlphaID, "texInFragColor");
	glUniform1i(uniformTexGColor, OGLTextureUnitID_GColor);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitGeometryProgram(const char *geometryVtxShaderCString, const char *geometryFragShaderCString,
													  const char *geometryAlphaVtxShaderCString, const char *geometryAlphaFragShaderCString,
													  const char *geometryMSAlphaVtxShaderCString, const char *geometryMSAlphaFragShaderCString)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	OGLRef.vertexGeometryShaderID = glCreateShader(GL_VERTEX_SHADER);
	if(!OGLRef.vertexGeometryShaderID)
	{
		INFO("OpenGL: Failed to create the vertex shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");		
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *vertexShaderProgramChar = geometryVtxShaderCString;
	glShaderSource(OGLRef.vertexGeometryShaderID, 1, (const GLchar **)&vertexShaderProgramChar, NULL);
	glCompileShader(OGLRef.vertexGeometryShaderID);
	if (!this->ValidateShaderCompile(OGLRef.vertexGeometryShaderID))
	{
		INFO("OpenGL: Failed to compile the vertex shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.fragmentGeometryShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if(!OGLRef.fragmentGeometryShaderID)
	{
		INFO("OpenGL: Failed to create the fragment shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *fragmentShaderProgramChar = geometryFragShaderCString;
	glShaderSource(OGLRef.fragmentGeometryShaderID, 1, (const GLchar **)&fragmentShaderProgramChar, NULL);
	glCompileShader(OGLRef.fragmentGeometryShaderID);
	if (!this->ValidateShaderCompile(OGLRef.fragmentGeometryShaderID))
	{
		INFO("OpenGL: Failed to compile the fragment shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.programGeometryID = glCreateProgram();
	if(!OGLRef.programGeometryID)
	{
		INFO("OpenGL: Failed to create the shader program. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glAttachShader(OGLRef.programGeometryID, OGLRef.vertexGeometryShaderID);
	glAttachShader(OGLRef.programGeometryID, OGLRef.fragmentGeometryShaderID);
	
	this->InitGeometryProgramBindings();
	
	glLinkProgram(OGLRef.programGeometryID);
	if (!this->ValidateShaderProgramLink(OGLRef.programGeometryID))
	{
		INFO("OpenGL: Failed to link the shader program. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.programGeometryID);
	
	this->InitGeometryProgramShaderLocations();
	
	// ------------------------------------------
	
	OGLRef.vtxShaderGeometryZeroDstAlphaID = glCreateShader(GL_VERTEX_SHADER);
	if(!OGLRef.vtxShaderGeometryZeroDstAlphaID)
	{
		INFO("OpenGL: Failed to create the vertex shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *vertexAlphaShaderProgramChar = geometryAlphaVtxShaderCString;
	glShaderSource(OGLRef.vtxShaderGeometryZeroDstAlphaID, 1, (const GLchar **)&vertexAlphaShaderProgramChar, NULL);
	glCompileShader(OGLRef.vtxShaderGeometryZeroDstAlphaID);
	if (!this->ValidateShaderCompile(OGLRef.vtxShaderGeometryZeroDstAlphaID))
	{
		INFO("OpenGL: Failed to compile the vertex shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.fragShaderGeometryZeroDstAlphaID = glCreateShader(GL_FRAGMENT_SHADER);
	if(!OGLRef.fragShaderGeometryZeroDstAlphaID)
	{
		INFO("OpenGL: Failed to create the fragment shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *fragmentAlphaShaderProgramChar = geometryAlphaFragShaderCString;
	glShaderSource(OGLRef.fragShaderGeometryZeroDstAlphaID, 1, (const GLchar **)&fragmentAlphaShaderProgramChar, NULL);
	glCompileShader(OGLRef.fragShaderGeometryZeroDstAlphaID);
	if (!this->ValidateShaderCompile(OGLRef.fragShaderGeometryZeroDstAlphaID))
	{
		INFO("OpenGL: Failed to compile the fragment shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.programGeometryZeroDstAlphaID = glCreateProgram();
	if(!OGLRef.programGeometryZeroDstAlphaID)
	{
		INFO("OpenGL: Failed to create the shader program. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glAttachShader(OGLRef.programGeometryZeroDstAlphaID, OGLRef.vtxShaderGeometryZeroDstAlphaID);
	glAttachShader(OGLRef.programGeometryZeroDstAlphaID, OGLRef.fragShaderGeometryZeroDstAlphaID);
	
	// ------------------------------------------
	
	bool useMSGeometryProgram = this->isSampleShadingSupported && (geometryMSAlphaVtxShaderCString != NULL) && (geometryMSAlphaFragShaderCString != NULL);
	
	if (useMSGeometryProgram)
	{
		OGLRef.vtxShaderMSGeometryZeroDstAlphaID = glCreateShader(GL_VERTEX_SHADER);
		if(!OGLRef.vtxShaderMSGeometryZeroDstAlphaID)
		{
			INFO("OpenGL: Failed to create the vertex shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
			return OGLERROR_SHADER_CREATE_ERROR;
		}
		
		const char *vertexMSAlphaShaderProgramChar = geometryMSAlphaVtxShaderCString;
		glShaderSource(OGLRef.vtxShaderMSGeometryZeroDstAlphaID, 1, (const GLchar **)&vertexMSAlphaShaderProgramChar, NULL);
		glCompileShader(OGLRef.vtxShaderMSGeometryZeroDstAlphaID);
		if (!this->ValidateShaderCompile(OGLRef.vtxShaderMSGeometryZeroDstAlphaID))
		{
			INFO("OpenGL: Failed to compile the vertex shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
			return OGLERROR_SHADER_CREATE_ERROR;
		}
		
		OGLRef.fragShaderMSGeometryZeroDstAlphaID = glCreateShader(GL_FRAGMENT_SHADER);
		if(!OGLRef.fragShaderMSGeometryZeroDstAlphaID)
		{
			INFO("OpenGL: Failed to create the fragment shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
			return OGLERROR_SHADER_CREATE_ERROR;
		}
		
		const char *fragmentMSAlphaShaderProgramChar = geometryMSAlphaFragShaderCString;
		glShaderSource(OGLRef.fragShaderMSGeometryZeroDstAlphaID, 1, (const GLchar **)&fragmentMSAlphaShaderProgramChar, NULL);
		glCompileShader(OGLRef.fragShaderMSGeometryZeroDstAlphaID);
		if (!this->ValidateShaderCompile(OGLRef.fragShaderMSGeometryZeroDstAlphaID))
		{
			INFO("OpenGL: Failed to compile the fragment shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
			return OGLERROR_SHADER_CREATE_ERROR;
		}
		
		OGLRef.programMSGeometryZeroDstAlphaID = glCreateProgram();
		if(!OGLRef.programMSGeometryZeroDstAlphaID)
		{
			INFO("OpenGL: Failed to create the shader program. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
			return OGLERROR_SHADER_CREATE_ERROR;
		}
		
		glAttachShader(OGLRef.programMSGeometryZeroDstAlphaID, OGLRef.vtxShaderMSGeometryZeroDstAlphaID);
		glAttachShader(OGLRef.programMSGeometryZeroDstAlphaID, OGLRef.fragShaderMSGeometryZeroDstAlphaID);
	}
	
	this->InitGeometryZeroDstAlphaProgramBindings();
	
	glLinkProgram(OGLRef.programGeometryZeroDstAlphaID);
	if (!this->ValidateShaderProgramLink(OGLRef.programGeometryZeroDstAlphaID))
	{
		INFO("OpenGL: Failed to link the shader program. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.programGeometryZeroDstAlphaID);
	
	if (useMSGeometryProgram)
	{
		glLinkProgram(OGLRef.programMSGeometryZeroDstAlphaID);
		if (!this->ValidateShaderProgramLink(OGLRef.programMSGeometryZeroDstAlphaID))
		{
			INFO("OpenGL: Failed to link the shader program. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
			return OGLERROR_SHADER_CREATE_ERROR;
		}
		
		glValidateProgram(OGLRef.programMSGeometryZeroDstAlphaID);
		
		this->willUsePerSampleZeroDstPass = true;
	}
	
	this->InitGeometryZeroDstAlphaProgramShaderLocations();
	
	// ------------------------------------------
	
	INFO("OpenGL: Successfully created shaders.\n");
	
	this->CreateToonTable();
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyGeometryProgram()
{
	if (!this->isShaderSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glUseProgram(0);
	
	glDetachShader(OGLRef.programGeometryID, OGLRef.vertexGeometryShaderID);
	glDetachShader(OGLRef.programGeometryID, OGLRef.fragmentGeometryShaderID);
	glDetachShader(OGLRef.programGeometryZeroDstAlphaID, OGLRef.vtxShaderGeometryZeroDstAlphaID);
	glDetachShader(OGLRef.programGeometryZeroDstAlphaID, OGLRef.fragShaderGeometryZeroDstAlphaID);
	
	glDeleteProgram(OGLRef.programGeometryID);
	glDeleteProgram(OGLRef.programGeometryZeroDstAlphaID);
	
	glDeleteShader(OGLRef.vertexGeometryShaderID);
	glDeleteShader(OGLRef.fragmentGeometryShaderID);
	glDeleteShader(OGLRef.vtxShaderGeometryZeroDstAlphaID);
	glDeleteShader(OGLRef.fragShaderGeometryZeroDstAlphaID);
	
	OGLRef.programGeometryID = 0;
	OGLRef.programGeometryZeroDstAlphaID = 0;
	
	OGLRef.vertexGeometryShaderID = 0;
	OGLRef.fragmentGeometryShaderID = 0;
	OGLRef.vtxShaderGeometryZeroDstAlphaID = 0;
	OGLRef.fragShaderGeometryZeroDstAlphaID = 0;
	
	this->DestroyToonTable();
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
	glGenTextures(1, &OGLRef.texCIColorID);
	glGenTextures(1, &OGLRef.texCIFogAttrID);
	glGenTextures(1, &OGLRef.texCIPolyID);
	glGenTextures(1, &OGLRef.texCIDepthStencilID);
	
	glGenTextures(1, &OGLRef.texGColorID);
	glGenTextures(1, &OGLRef.texGFogAttrID);
	glGenTextures(1, &OGLRef.texGPolyID);
	glGenTextures(1, &OGLRef.texGDepthStencilID);
	glGenTextures(1, &OGLRef.texGDepthStencilAlphaID);
	glGenTextures(1, &OGLRef.texZeroAlphaPixelMaskID);
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_DepthStencil);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthStencilID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, this->_framebufferWidth, this->_framebufferHeight, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_GColor);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_GPolyID);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGPolyID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ZeroAlphaPixelMask);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texZeroAlphaPixelMaskID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_FogAttr);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGFogAttrID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
	if (this->isShaderSupported && this->isVBOSupported)
	{
		glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthStencilAlphaID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, this->_framebufferWidth, this->_framebufferHeight, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
	}
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIDepthStencilID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIPolyID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIFogAttrID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	// Set up FBOs
	glGenFramebuffersEXT(1, &OGLRef.fboClearImageID);
	glGenFramebuffersEXT(1, &OGLRef.fboRenderID);
	glGenFramebuffersEXT(1, &OGLRef.fboRenderAlphaID);
	glGenFramebuffersEXT(1, &OGLRef.fboPostprocessID);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboClearImageID);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, OGLRef.texCIColorID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, OGLRef.texCIPolyID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_TEXTURE_2D, OGLRef.texCIFogAttrID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texCIDepthStencilID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texCIDepthStencilID, 0);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to create FBOs!\n");
		this->DestroyFBOs();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	if (this->isShaderSupported)
	{
		glDrawBuffers(3, RenderDrawList);
	}
	else
	{
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	}
	
	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, OGLRef.texGColorID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, OGLRef.texGPolyID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_TEXTURE_2D, OGLRef.texGFogAttrID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texGDepthStencilID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texGDepthStencilID, 0);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to create FBOs!\n");
		this->DestroyFBOs();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	if (this->isShaderSupported)
	{
		glDrawBuffers(3, RenderDrawList);
	}
	else
	{
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	}
	
	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	
	if (this->isShaderSupported && this->isVBOSupported)
	{
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderAlphaID);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, OGLRef.texGColorID, 0);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, OGLRef.texGPolyID, 0);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_TEXTURE_2D, OGLRef.texGFogAttrID, 0);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texGDepthStencilAlphaID, 0);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texGDepthStencilAlphaID, 0);
		
		if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
		{
			INFO("OpenGL: Failed to create FBOs!\n");
			this->DestroyFBOs();
			
			return OGLERROR_FBO_CREATE_ERROR;
		}
		
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	}
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboPostprocessID);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, OGLRef.texGColorID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, OGLRef.texFinalColorID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_TEXTURE_2D, OGLRef.texZeroAlphaPixelMaskID, 0);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to created FBOs!\n");
		this->DestroyFBOs();
		
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
	glDeleteFramebuffersEXT(1, &OGLRef.fboClearImageID);
	glDeleteFramebuffersEXT(1, &OGLRef.fboRenderID);
	glDeleteFramebuffersEXT(1, &OGLRef.fboRenderAlphaID);
	glDeleteFramebuffersEXT(1, &OGLRef.fboPostprocessID);
	glDeleteTextures(1, &OGLRef.texCIColorID);
	glDeleteTextures(1, &OGLRef.texCIFogAttrID);
	glDeleteTextures(1, &OGLRef.texCIPolyID);
	glDeleteTextures(1, &OGLRef.texCIDepthStencilID);
	glDeleteTextures(1, &OGLRef.texGColorID);
	glDeleteTextures(1, &OGLRef.texGPolyID);
	glDeleteTextures(1, &OGLRef.texGFogAttrID);
	glDeleteTextures(1, &OGLRef.texGDepthStencilID);
	glDeleteTextures(1, &OGLRef.texGDepthStencilAlphaID);
	glDeleteTextures(1, &OGLRef.texZeroAlphaPixelMaskID);
	
	OGLRef.fboClearImageID = 0;
	OGLRef.fboRenderID = 0;
	OGLRef.fboRenderAlphaID = 0;
	OGLRef.fboPostprocessID = 0;
	
	this->isFBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreateMultisampledFBO(GLsizei numSamples)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up FBO render targets
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGColorID);
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGPolyID);
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGFogAttrID);
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGDepthStencilID);
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGDepthStencilAlphaID);
	
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGColorID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGPolyID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGFogAttrID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_DEPTH24_STENCIL8_EXT, this->_framebufferWidth, this->_framebufferHeight);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilAlphaID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_DEPTH24_STENCIL8_EXT, this->_framebufferWidth, this->_framebufferHeight);
	
	// Set up multisampled rendering FBO
	glGenFramebuffersEXT(1, &OGLRef.fboMSIntermediateRenderID);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboMSIntermediateRenderID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGColorID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGPolyID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGFogAttrID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilID);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to create multisampled FBO!\n");
		this->DestroyMultisampledFBO();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glGenFramebuffersEXT(1, &OGLRef.fboMSIntermediateRenderAlphaID);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboMSIntermediateRenderAlphaID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGColorID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGPolyID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGFogAttrID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilAlphaID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilAlphaID);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to create multisampled FBO!\n");
		this->DestroyMultisampledFBO();
		
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
	glDeleteFramebuffersEXT(1, &OGLRef.fboMSIntermediateRenderAlphaID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGColorID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGPolyID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGFogAttrID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGDepthStencilID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGDepthStencilAlphaID);
	
	OGLRef.fboMSIntermediateRenderID = 0;
	OGLRef.fboMSIntermediateRenderAlphaID = 0;
	
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
	OGLRef.stateTexMirroredRepeat = (isTexMirroredRepeatSupported) ? GL_MIRRORED_REPEAT : GL_REPEAT;
	
	// Map the vertex list's colors with 4 floats per color. This is being done
	// because OpenGL needs 4-colors per vertex to support translucency. (The DS
	// uses 3-colors per vertex, and adds alpha through the poly, so we can't
	// simply reference the colors+alpha from just the vertices by themselves.)
	OGLRef.color4fBuffer = (this->isShaderSupported) ? NULL : new GLfloat[VERTLIST_SIZE * 4];
	
	// If VBOs aren't supported, then we need to create the index buffer on the
	// client side so that we have a buffer to update.
	OGLRef.vertIndexBuffer = (this->isVBOSupported) ? NULL : new GLushort[OGLRENDER_VERT_INDEX_BUFFER_COUNT];
	
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

Render3DError OpenGLRenderer_1_2::InitPostprocessingPrograms(const char *edgeMarkVtxShaderCString,
															 const char *edgeMarkFragShaderCString,
															 const char *fogVtxShaderCString,
															 const char *fogFragShaderCString,
															 const char *framebufferOutputVtxShaderCString,
															 const char *framebufferOutputRGBA6665FragShaderCString,
															 const char *framebufferOutputRGBA8888FragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	OGLRef.vertexEdgeMarkShaderID = glCreateShader(GL_VERTEX_SHADER);
	if(!OGLRef.vertexEdgeMarkShaderID)
	{
		INFO("OpenGL: Failed to create the edge mark vertex shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *edgeMarkVtxShaderCStr = edgeMarkVtxShaderCString;
	glShaderSource(OGLRef.vertexEdgeMarkShaderID, 1, (const GLchar **)&edgeMarkVtxShaderCStr, NULL);
	glCompileShader(OGLRef.vertexEdgeMarkShaderID);
	if (!this->ValidateShaderCompile(OGLRef.vertexEdgeMarkShaderID))
	{
		INFO("OpenGL: Failed to compile the edge mark vertex shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.fragmentEdgeMarkShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if(!OGLRef.fragmentEdgeMarkShaderID)
	{
		INFO("OpenGL: Failed to create the edge mark fragment shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *edgeMarkFragShaderCStr = edgeMarkFragShaderCString;
	glShaderSource(OGLRef.fragmentEdgeMarkShaderID, 1, (const GLchar **)&edgeMarkFragShaderCStr, NULL);
	glCompileShader(OGLRef.fragmentEdgeMarkShaderID);
	if (!this->ValidateShaderCompile(OGLRef.fragmentEdgeMarkShaderID))
	{
		INFO("OpenGL: Failed to compile the edge mark fragment shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.programEdgeMarkID = glCreateProgram();
	if(!OGLRef.programEdgeMarkID)
	{
		INFO("OpenGL: Failed to create the edge mark shader program.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glAttachShader(OGLRef.programEdgeMarkID, OGLRef.vertexEdgeMarkShaderID);
	glAttachShader(OGLRef.programEdgeMarkID, OGLRef.fragmentEdgeMarkShaderID);
	
	error = this->InitEdgeMarkProgramBindings();
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL: Failed to make the edge mark shader bindings.\n");
		return error;
	}
	
	glLinkProgram(OGLRef.programEdgeMarkID);
	if (!this->ValidateShaderProgramLink(OGLRef.programEdgeMarkID))
	{
		INFO("OpenGL: Failed to link the edge mark shader program.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.programEdgeMarkID);
	this->InitEdgeMarkProgramShaderLocations();
	
	// ------------------------------------------
	
	OGLRef.vertexFogShaderID = glCreateShader(GL_VERTEX_SHADER);
	if(!OGLRef.vertexFogShaderID)
	{
		INFO("OpenGL: Failed to create the fog vertex shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *fogVtxShaderCStr = fogVtxShaderCString;
	glShaderSource(OGLRef.vertexFogShaderID, 1, (const GLchar **)&fogVtxShaderCStr, NULL);
	glCompileShader(OGLRef.vertexFogShaderID);
	if (!this->ValidateShaderCompile(OGLRef.vertexFogShaderID))
	{
		INFO("OpenGL: Failed to compile the fog vertex shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.fragmentFogShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if(!OGLRef.fragmentFogShaderID)
	{
		INFO("OpenGL: Failed to create the fog fragment shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *fogFragShaderCStr = fogFragShaderCString;
	glShaderSource(OGLRef.fragmentFogShaderID, 1, (const GLchar **)&fogFragShaderCStr, NULL);
	glCompileShader(OGLRef.fragmentFogShaderID);
	if (!this->ValidateShaderCompile(OGLRef.fragmentFogShaderID))
	{
		INFO("OpenGL: Failed to compile the fog fragment shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.programFogID = glCreateProgram();
	if(!OGLRef.programFogID)
	{
		INFO("OpenGL: Failed to create the fog shader program.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glAttachShader(OGLRef.programFogID, OGLRef.vertexFogShaderID);
	glAttachShader(OGLRef.programFogID, OGLRef.fragmentFogShaderID);
	
	error = this->InitFogProgramBindings();
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL: Failed to make the fog shader bindings.\n");
		return error;
	}
	
	glLinkProgram(OGLRef.programFogID);
	if (!this->ValidateShaderProgramLink(OGLRef.programFogID))
	{
		INFO("OpenGL: Failed to link the fog shader program.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.programFogID);
	this->InitFogProgramShaderLocations();
	
	// ------------------------------------------
	
	OGLRef.vertexFramebufferOutputShaderID = glCreateShader(GL_VERTEX_SHADER);
	if(!OGLRef.vertexFramebufferOutputShaderID)
	{
		INFO("OpenGL: Failed to create the framebuffer output vertex shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *framebufferOutputVtxShaderCStr = framebufferOutputVtxShaderCString;
	glShaderSource(OGLRef.vertexFramebufferOutputShaderID, 1, (const GLchar **)&framebufferOutputVtxShaderCStr, NULL);
	glCompileShader(OGLRef.vertexFramebufferOutputShaderID);
	if (!this->ValidateShaderCompile(OGLRef.vertexFramebufferOutputShaderID))
	{
		INFO("OpenGL: Failed to compile the framebuffer output vertex shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.fragmentFramebufferRGBA6665OutputShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if(!OGLRef.fragmentFramebufferRGBA6665OutputShaderID)
	{
		INFO("OpenGL: Failed to create the framebuffer output fragment shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.fragmentFramebufferRGBA8888OutputShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if(!OGLRef.fragmentFramebufferRGBA8888OutputShaderID)
	{
		INFO("OpenGL: Failed to create the framebuffer output fragment shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *framebufferOutputRGBA6665FragShaderCStr = framebufferOutputRGBA6665FragShaderCString;
	glShaderSource(OGLRef.fragmentFramebufferRGBA6665OutputShaderID, 1, (const GLchar **)&framebufferOutputRGBA6665FragShaderCStr, NULL);
	glCompileShader(OGLRef.fragmentFramebufferRGBA6665OutputShaderID);
	if (!this->ValidateShaderCompile(OGLRef.fragmentFramebufferRGBA6665OutputShaderID))
	{
		INFO("OpenGL: Failed to compile the framebuffer output fragment shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *framebufferOutputRGBA8888FragShaderCStr = framebufferOutputRGBA8888FragShaderCString;
	glShaderSource(OGLRef.fragmentFramebufferRGBA8888OutputShaderID, 1, (const GLchar **)&framebufferOutputRGBA8888FragShaderCStr, NULL);
	glCompileShader(OGLRef.fragmentFramebufferRGBA8888OutputShaderID);
	if (!this->ValidateShaderCompile(OGLRef.fragmentFramebufferRGBA8888OutputShaderID))
	{
		INFO("OpenGL: Failed to compile the framebuffer output fragment shader.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.programFramebufferRGBA6665OutputID = glCreateProgram();
	if(!OGLRef.programFramebufferRGBA6665OutputID)
	{
		INFO("OpenGL: Failed to create the framebuffer output shader program.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.programFramebufferRGBA8888OutputID = glCreateProgram();
	if(!OGLRef.programFramebufferRGBA8888OutputID)
	{
		INFO("OpenGL: Failed to create the framebuffer output shader program.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glAttachShader(OGLRef.programFramebufferRGBA6665OutputID, OGLRef.vertexFramebufferOutputShaderID);
	glAttachShader(OGLRef.programFramebufferRGBA6665OutputID, OGLRef.fragmentFramebufferRGBA6665OutputShaderID);
	glAttachShader(OGLRef.programFramebufferRGBA8888OutputID, OGLRef.vertexFramebufferOutputShaderID);
	glAttachShader(OGLRef.programFramebufferRGBA8888OutputID, OGLRef.fragmentFramebufferRGBA8888OutputShaderID);
	
	error = this->InitFramebufferOutputProgramBindings();
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL: Failed to make the framebuffer output shader bindings.\n");
		return error;
	}
	
	glLinkProgram(OGLRef.programFramebufferRGBA6665OutputID);
	glLinkProgram(OGLRef.programFramebufferRGBA8888OutputID);
	
	if (!this->ValidateShaderProgramLink(OGLRef.programFramebufferRGBA6665OutputID) || !this->ValidateShaderProgramLink(OGLRef.programFramebufferRGBA8888OutputID))
	{
		INFO("OpenGL: Failed to link the framebuffer output shader program.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.programFramebufferRGBA6665OutputID);
	glValidateProgram(OGLRef.programFramebufferRGBA8888OutputID);
	
	this->InitFramebufferOutputShaderLocations();
	
	// ------------------------------------------
	
	glUseProgram(OGLRef.programGeometryID);
	INFO("OpenGL: Successfully created postprocess shaders.\n");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DestroyPostprocessingPrograms()
{
	if (!this->isShaderSupported)
	{
		return OGLERROR_NOERR;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glUseProgram(0);
	
	glDetachShader(OGLRef.programEdgeMarkID, OGLRef.vertexEdgeMarkShaderID);
	glDetachShader(OGLRef.programEdgeMarkID, OGLRef.fragmentEdgeMarkShaderID);
	glDetachShader(OGLRef.programFogID, OGLRef.vertexFogShaderID);
	glDetachShader(OGLRef.programFogID, OGLRef.fragmentFogShaderID);
	glDetachShader(OGLRef.programFramebufferRGBA6665OutputID, OGLRef.vertexFramebufferOutputShaderID);
	glDetachShader(OGLRef.programFramebufferRGBA6665OutputID, OGLRef.fragmentFramebufferRGBA6665OutputShaderID);
	glDetachShader(OGLRef.programFramebufferRGBA8888OutputID, OGLRef.vertexFramebufferOutputShaderID);
	glDetachShader(OGLRef.programFramebufferRGBA8888OutputID, OGLRef.fragmentFramebufferRGBA8888OutputShaderID);
	
	glDeleteProgram(OGLRef.programEdgeMarkID);
	glDeleteProgram(OGLRef.programFogID);
	glDeleteProgram(OGLRef.programFramebufferRGBA6665OutputID);
	glDeleteProgram(OGLRef.programFramebufferRGBA8888OutputID);
	
	glDeleteShader(OGLRef.vertexEdgeMarkShaderID);
	glDeleteShader(OGLRef.fragmentEdgeMarkShaderID);
	glDeleteShader(OGLRef.vertexFogShaderID);
	glDeleteShader(OGLRef.fragmentFogShaderID);
	glDeleteShader(OGLRef.vertexFramebufferOutputShaderID);
	glDeleteShader(OGLRef.fragmentFramebufferRGBA6665OutputShaderID);
	glDeleteShader(OGLRef.fragmentFramebufferRGBA8888OutputShaderID);
	
	OGLRef.programEdgeMarkID = 0;
	OGLRef.programFogID = 0;
	OGLRef.programFramebufferRGBA6665OutputID = 0;
	OGLRef.programFramebufferRGBA8888OutputID = 0;
	
	OGLRef.vertexEdgeMarkShaderID = 0;
	OGLRef.fragmentEdgeMarkShaderID = 0;
	OGLRef.vertexFogShaderID = 0;
	OGLRef.fragmentFogShaderID = 0;
	OGLRef.vertexFramebufferOutputShaderID = 0;
	OGLRef.fragmentFramebufferRGBA6665OutputShaderID = 0;
	OGLRef.fragmentFramebufferRGBA8888OutputShaderID = 0;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitEdgeMarkProgramBindings()
{
	OGLRenderRef &OGLRef = *this->ref;
	glBindAttribLocation(OGLRef.programEdgeMarkID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programEdgeMarkID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitEdgeMarkProgramShaderLocations()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glUseProgram(OGLRef.programEdgeMarkID);
	
	const GLint uniformTexGDepth				= glGetUniformLocation(OGLRef.programEdgeMarkID, "texInFragDepth");
	const GLint uniformTexGPolyID				= glGetUniformLocation(OGLRef.programEdgeMarkID, "texInPolyID");
	glUniform1i(uniformTexGDepth, OGLTextureUnitID_DepthStencil);
	glUniform1i(uniformTexGPolyID, OGLTextureUnitID_GPolyID);
	
	OGLRef.uniformFramebufferSize	= glGetUniformLocation(OGLRef.programEdgeMarkID, "framebufferSize");
	OGLRef.uniformStateEdgeColor	= glGetUniformLocation(OGLRef.programEdgeMarkID, "stateEdgeColor");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitFogProgramBindings()
{
	OGLRenderRef &OGLRef = *this->ref;
	glBindAttribLocation(OGLRef.programFogID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programFogID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitFogProgramShaderLocations()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glUseProgram(OGLRef.programFogID);
	
	const GLint uniformTexGColor			= glGetUniformLocation(OGLRef.programFogID, "texInFragColor");
	const GLint uniformTexGDepth			= glGetUniformLocation(OGLRef.programFogID, "texInFragDepth");
	const GLint uniformTexGFog				= glGetUniformLocation(OGLRef.programFogID, "texInFogAttributes");
	glUniform1i(uniformTexGColor, OGLTextureUnitID_GColor);
	glUniform1i(uniformTexGDepth, OGLTextureUnitID_DepthStencil);
	glUniform1i(uniformTexGFog, OGLTextureUnitID_FogAttr);
	
	OGLRef.uniformStateEnableFogAlphaOnly	= glGetUniformLocation(OGLRef.programFogID, "stateEnableFogAlphaOnly");
	OGLRef.uniformStateFogColor				= glGetUniformLocation(OGLRef.programFogID, "stateFogColor");
	OGLRef.uniformStateFogDensity			= glGetUniformLocation(OGLRef.programFogID, "stateFogDensity");
	OGLRef.uniformStateFogOffset			= glGetUniformLocation(OGLRef.programFogID, "stateFogOffset");
	OGLRef.uniformStateFogStep				= glGetUniformLocation(OGLRef.programFogID, "stateFogStep");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitFramebufferOutputProgramBindings()
{
	OGLRenderRef &OGLRef = *this->ref;
	glBindAttribLocation(OGLRef.programFramebufferRGBA6665OutputID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programFramebufferRGBA6665OutputID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	glBindAttribLocation(OGLRef.programFramebufferRGBA8888OutputID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programFramebufferRGBA8888OutputID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitFramebufferOutputShaderLocations()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glUseProgram(OGLRef.programFramebufferRGBA6665OutputID);
	OGLRef.uniformFramebufferSize_ConvertRGBA6665 = glGetUniformLocation(OGLRef.programFramebufferRGBA6665OutputID, "framebufferSize");
	OGLRef.uniformTexInFragColor_ConvertRGBA6665 = glGetUniformLocation(OGLRef.programFramebufferRGBA6665OutputID, "texInFragColor");
	glUniform1i(OGLRef.uniformTexInFragColor_ConvertRGBA6665, OGLTextureUnitID_FinalColor);
	
	glUseProgram(OGLRef.programFramebufferRGBA8888OutputID);
	OGLRef.uniformFramebufferSize_ConvertRGBA8888 = glGetUniformLocation(OGLRef.programFramebufferRGBA8888OutputID, "framebufferSize");
	OGLRef.uniformTexInFragColor_ConvertRGBA8888 = glGetUniformLocation(OGLRef.programFramebufferRGBA8888OutputID, "texInFragColor");
	glUniform1i(OGLRef.uniformTexInFragColor_ConvertRGBA8888, OGLTextureUnitID_FinalColor);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::CreateToonTable()
{
	OGLRenderRef &OGLRef = *this->ref;
	u16 tempToonTable[32];
	memset(tempToonTable, 0, sizeof(tempToonTable));
	
	// The toon table is a special 1D texture where each pixel corresponds
	// to a specific color in the toon table.
	glGenTextures(1, &OGLRef.texToonTableID);
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ToonTable);
	glBindTexture(GL_TEXTURE_1D, OGLRef.texToonTableID);
	
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, tempToonTable);
	
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DestroyToonTable()
{
	glDeleteTextures(1, &this->ref->texToonTableID);
	this->ref->texToonTableID = 0;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::UploadClearImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 *__restrict polyIDBuffer)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isShaderSupported)
	{
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT; i++)
		{
			OGLRef.workingCIDepthStencilBuffer[i] = (depthBuffer[i] << 8) | polyIDBuffer[i];
			OGLRef.workingCIFogAttributesBuffer[i] = (fogBuffer[i]) ? 0xFF0000FF : 0xFF000000;
			OGLRef.workingCIPolyIDBuffer[i] = (GLuint)polyIDBuffer[i] | 0xFF000000;
		}
	}
	else
	{
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT; i++)
		{
			OGLRef.workingCIDepthStencilBuffer[i] = (depthBuffer[i] << 8) | polyIDBuffer[i];
		}
	}
	
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIColorID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, colorBuffer);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIDepthStencilID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, OGLRef.workingCIDepthStencilBuffer);
	
	if (this->isShaderSupported)
	{
		glBindTexture(GL_TEXTURE_2D, OGLRef.texCIFogAttrID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, OGLRef.workingCIFogAttributesBuffer);
		
		glBindTexture(GL_TEXTURE_2D, OGLRef.texCIPolyID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, OGLRef.workingCIPolyIDBuffer);
	}
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
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

Render3DError OpenGLRenderer_1_2::EnableVertexAttributes()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(OGLRef.vaoGeometryStatesID);
	}
	else
	{
		if (this->isShaderSupported)
		{
			glEnableVertexAttribArray(OGLVertexAttributeID_Position);
			glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
			glEnableVertexAttribArray(OGLVertexAttributeID_Color);
			glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), OGLRef.vtxPtrPosition);
			glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), OGLRef.vtxPtrTexCoord);
			glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), OGLRef.vtxPtrColor);
		}
		else
		{
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glEnableClientState(GL_VERTEX_ARRAY);
			
			if (this->isVBOSupported)
			{
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
				glColorPointer(4, GL_FLOAT, 0, OGLRef.vtxPtrColor);
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboGeometryVtxID);
			}
			else
			{
				glColorPointer(4, GL_FLOAT, 0, OGLRef.vtxPtrColor);
			}
			
			glVertexPointer(4, GL_FLOAT, sizeof(VERT), OGLRef.vtxPtrPosition);
			glTexCoordPointer(2, GL_FLOAT, sizeof(VERT), OGLRef.vtxPtrTexCoord);
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
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ZeroDstAlphaPass(const POLYLIST *polyList, const INDEXLIST *indexList, bool enableAlphaBlending, size_t indexOffset, POLYGON_ATTR lastPolyAttr)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->isShaderSupported || !this->isFBOSupported || !this->isVBOSupported)
	{
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	
	// Pre Pass: Fill in the stencil buffer based on the alpha of the current framebuffer color.
	// Fully transparent pixels (alpha == 0) -- Set stencil buffer to 0
	// All other pixels (alpha != 0) -- Set stencil buffer to 1
	
	this->DisableVertexAttributes();
	
	const bool isRunningMSAA = this->isMultisampledFBOSupported && (OGLRef.selectedRenderingFBO == OGLRef.fboMSIntermediateRenderID);
	
	if (isRunningMSAA)
	{
		// Just downsample the color buffer now so that we have some texture data to sample from in the non-multisample shader.
		// Not perfectly pixel accurate, but it's better than nothing.
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.fboRenderAlphaID);
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glBlitFramebufferEXT(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, (isRunningMSAA) ? OGLRef.fboMSIntermediateRenderAlphaID : OGLRef.fboRenderAlphaID);
	glDrawBuffer(GL_NONE);
	glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT);
	glBlitFramebufferEXT(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	
	glUseProgram(OGLRef.programGeometryZeroDstAlphaID);
	glViewport(0, 0, this->_framebufferWidth, this->_framebufferHeight);
	glDisable(GL_BLEND);
	glEnable(GL_STENCIL_TEST);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	
	glStencilFunc(GL_ALWAYS, 0x80, 0x80);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilMask(0x80);
	
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
	
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(0);
	}
	else
	{
		glDisableVertexAttribArray(OGLVertexAttributeID_Position);
		glDisableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	}
	
	// Setup for multiple pass alpha poly drawing
	glUseProgram(OGLRef.programGeometryID);
	glUniform1i(OGLRef.uniformTexDrawOpaque, GL_FALSE);
	glUniform1i(OGLRef.uniformPolyDrawShadow, GL_FALSE);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
	this->EnableVertexAttributes();
	
	// Draw the alpha polys, touching fully transparent pixels only once.
	static const GLenum RenderAlphaDrawList[3] = {GL_COLOR_ATTACHMENT0_EXT, GL_NONE, GL_NONE};
	glDrawBuffers(3, RenderAlphaDrawList);
	glEnable(GL_DEPTH_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	glDepthMask(GL_FALSE);
	glStencilFunc(GL_NOTEQUAL, 0x80, 0x80);
	
	this->DrawPolygonsForIndexRange<OGLPolyDrawMode_ZeroAlphaPass>(polyList, indexList, polyList->opaqueCount, polyList->count - 1, indexOffset, lastPolyAttr);
	
	// Restore OpenGL states back to normal.
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
	glDrawBuffers(3, RenderDrawList);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	
	if (enableAlphaBlending)
	{
		glEnable(GL_BLEND);
	}
	else
	{
		glDisable(GL_BLEND);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DownsampleFBO()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isMultisampledFBOSupported && (OGLRef.selectedRenderingFBO == OGLRef.fboMSIntermediateRenderID))
	{
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboMSIntermediateRenderID);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
		
		if (this->isShaderSupported)
		{
			// Blit the color and depth buffers
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glBlitFramebufferEXT(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
			
			// Blit the polygon ID buffer
			glReadBuffer(GL_COLOR_ATTACHMENT1_EXT);
			glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);
			glBlitFramebufferEXT(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			
			// Blit the fog buffer
			glReadBuffer(GL_COLOR_ATTACHMENT2_EXT);
			glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
			glBlitFramebufferEXT(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			
			// Reset framebuffer targets
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glDrawBuffers(3, RenderDrawList);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
		}
		else
		{
			// Blit the color buffer
			glBlitFramebufferEXT(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ReadBackPixels()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->_mappedFramebuffer != NULL)
	{
		glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
		this->_mappedFramebuffer = NULL;
	}
	
	if (this->willFlipAndConvertFramebufferOnGPU)
	{
		// Both flips and converts the framebuffer on the GPU. No additional postprocessing
		// should be necessary at this point.
		const GLuint convertProgramID = (this->_outputFormat == NDSColorFormat_BGR666_Rev) ? OGLRef.programFramebufferRGBA6665OutputID : OGLRef.programFramebufferRGBA8888OutputID;
		const GLint uniformTexNumber = (this->_outputFormat == NDSColorFormat_BGR666_Rev) ? OGLRef.uniformTexInFragColor_ConvertRGBA6665 : OGLRef.uniformTexInFragColor_ConvertRGBA8888;
		const GLint uniformFramebufferSize = (this->_outputFormat == NDSColorFormat_BGR666_Rev) ? OGLRef.uniformFramebufferSize_ConvertRGBA6665 : OGLRef.uniformFramebufferSize_ConvertRGBA8888;
		
		glUseProgram(convertProgramID);
		
		if (this->isFBOSupported)
		{
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboPostprocessID);
			
			if (this->_lastTextureDrawTarget == OGLTextureUnitID_GColor)
			{
				glUniform1i(uniformTexNumber, OGLTextureUnitID_GColor);
				glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);
				glReadBuffer(GL_COLOR_ATTACHMENT1_EXT);
				this->_lastTextureDrawTarget = OGLTextureUnitID_FinalColor;
			}
			else
			{
				glUniform1i(uniformTexNumber, OGLTextureUnitID_FinalColor);
				glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
				glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
				this->_lastTextureDrawTarget = OGLTextureUnitID_GColor;
			}
		}
		else
		{
			glUniform1i(uniformTexNumber, OGLTextureUnitID_FinalColor);
			glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_FinalColor);
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, this->_framebufferWidth, this->_framebufferHeight);
			glActiveTextureARB(GL_TEXTURE0_ARB);
		}
		
		glUniform2f(uniformFramebufferSize, this->_framebufferWidth, this->_framebufferHeight);
		glViewport(0, 0, this->_framebufferWidth, this->_framebufferHeight);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		
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
		
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
		
		if (this->isVAOSupported)
		{
			glBindVertexArray(0);
		}
		else
		{
			glDisableVertexAttribArray(OGLVertexAttributeID_Position);
			glDisableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
		}
	}
	else if (this->willFlipOnlyFramebufferOnGPU)
	{
		// Just flips the framebuffer in Y to match the coordinates of OpenGL and the NDS hardware.
		// Further colorspace conversion will need to be done in a later step.
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.fboPostprocessID);
		
		if (this->_lastTextureDrawTarget == OGLTextureUnitID_GColor)
		{
			glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);
			glBlitFramebufferEXT(0, this->_framebufferHeight, this->_framebufferWidth, 0, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboPostprocessID);
			glReadBuffer(GL_COLOR_ATTACHMENT1_EXT);
		}
		else
		{
			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glBlitFramebufferEXT(0, this->_framebufferHeight, this->_framebufferWidth, 0, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboPostprocessID);
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		}
	}
	
	if (this->isPBOSupported)
	{
		// Read back the pixels in BGRA format, since legacy OpenGL devices may experience a performance
		// penalty if the readback is in any other format.
		glReadPixels(0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_BGRA, GL_UNSIGNED_BYTE, 0);
	}
	
	this->_pixelReadNeedsFinish = true;
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::BeginRender(const GFX3D &engine)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if(!BEGINGL())
	{
		return OGLERROR_BEGINGL_FAILED;
	}
	
	if (this->isShaderSupported)
	{
		glUseProgram(OGLRef.programGeometryID);
		glUniform1i(OGLRef.uniformStateToonShadingMode, engine.renderState.shading);
		glUniform1i(OGLRef.uniformStateEnableAlphaTest, (engine.renderState.enableAlphaTest) ? GL_TRUE : GL_FALSE);
		glUniform1i(OGLRef.uniformStateEnableAntialiasing, (engine.renderState.enableAntialiasing) ? GL_TRUE : GL_FALSE);
		glUniform1i(OGLRef.uniformStateEnableEdgeMarking, (this->_enableEdgeMark) ? GL_TRUE : GL_FALSE);
		glUniform1i(OGLRef.uniformStateUseWDepth, (engine.renderState.wbuffer) ? GL_TRUE : GL_FALSE);
		glUniform1f(OGLRef.uniformStateAlphaTestRef, divide5bitBy31_LUT[engine.renderState.alphaTestRef]);
		glUniform1i(OGLRef.uniformTexDrawOpaque, GL_FALSE);
		glUniform1i(OGLRef.uniformPolyDrawShadow, GL_FALSE);
	}
	else
	{
		if(engine.renderState.enableAlphaTest && (engine.renderState.alphaTestRef > 0))
		{
			glAlphaFunc(GL_GEQUAL, divide5bitBy31_LUT[engine.renderState.alphaTestRef]);
		}
		else
		{
			glAlphaFunc(GL_GREATER, 0);
		}
		
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
	}
	
	GLushort *indexPtr = NULL;
	
	if (this->isVBOSupported)
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboGeometryVtxID);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboGeometryIndexID);
		indexPtr = (GLushort *)glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
	}
	else
	{
		// If VBOs aren't supported, we need to use the client-side buffers here.
		OGLRef.vtxPtrPosition = &engine.vertList[0].coord;
		OGLRef.vtxPtrTexCoord = &engine.vertList[0].texcoord;
		OGLRef.vtxPtrColor = (this->isShaderSupported) ? (GLvoid *)&engine.vertList[0].color : OGLRef.color4fBuffer;
		indexPtr = OGLRef.vertIndexBuffer;
	}
	
	size_t vertIndexCount = 0;
	
	for (size_t i = 0; i < engine.polylist->count; i++)
	{
		const POLY *thePoly = &engine.polylist->list[engine.indexlist.list[i]];
		const size_t polyType = thePoly->type;
		
		if (this->isShaderSupported)
		{
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
		else
		{
			const GLfloat thePolyAlpha = (thePoly->isWireframe()) ? 1.0f : divide5bitBy31_LUT[thePoly->attribute.Alpha];
			
			for (size_t j = 0; j < polyType; j++)
			{
				const GLushort vertIndex = thePoly->vertIndexes[j];
				const size_t colorIndex = vertIndex * 4;
				
				// Consolidate the vertex color and the poly alpha to our internal color buffer
				// so that OpenGL can use it.
				const VERT *vert = &engine.vertList[vertIndex];
				OGLRef.color4fBuffer[colorIndex+0] = material_8bit_to_float[vert->color[0]];
				OGLRef.color4fBuffer[colorIndex+1] = material_8bit_to_float[vert->color[1]];
				OGLRef.color4fBuffer[colorIndex+2] = material_8bit_to_float[vert->color[2]];
				OGLRef.color4fBuffer[colorIndex+3] = thePolyAlpha;
				
				// While we're looping through our vertices, add each vertex index to a
				// buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
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
		
		this->_textureList[i] = this->GetLoadedTextureFromPolygon(*thePoly, this->_enableTextureSampling);
	}
	
	if (this->isVBOSupported)
	{
		glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
		glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VERT) * engine.vertListCount, engine.vertList);
	}
	
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	
	this->_needsZeroDstAlphaPass = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::RenderGeometry(const GFX3D_State &renderState, const POLYLIST *polyList, const INDEXLIST *indexList)
{
	if (polyList->count > 0)
	{
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_STENCIL_TEST);
		
		if (renderState.enableAlphaBlending)
		{
			glEnable(GL_BLEND);
		}
		else
		{
			glDisable(GL_BLEND);
		}
		
		glActiveTextureARB(GL_TEXTURE0_ARB);
		
		this->EnableVertexAttributes();
		
		size_t indexOffset = 0;
		
		const POLY &firstPoly = polyList->list[indexList->list[0]];
		POLYGON_ATTR lastPolyAttr = firstPoly.attribute;
		
		if (polyList->opaqueCount > 0)
		{
			this->SetupPolygon(firstPoly, false, true);
			this->DrawPolygonsForIndexRange<OGLPolyDrawMode_DrawOpaquePolys>(polyList, indexList, 0, polyList->opaqueCount - 1, indexOffset, lastPolyAttr);
		}
		
		if (polyList->opaqueCount < polyList->count)
		{
			if (this->_needsZeroDstAlphaPass)
			{
				if (polyList->opaqueCount == 0)
				{
					this->SetupPolygon(firstPoly, true, false);
				}
				
				this->ZeroDstAlphaPass(polyList, indexList, renderState.enableAlphaBlending, indexOffset, lastPolyAttr);
				
				if (polyList->opaqueCount > 0)
				{
					const POLY &lastOpaquePoly = polyList->list[indexList->list[polyList->opaqueCount - 1]];
					lastPolyAttr = lastOpaquePoly.attribute;
					this->SetupPolygon(lastOpaquePoly, false, true);
				}
			}
			
			if (polyList->opaqueCount == 0)
			{
				this->SetupPolygon(firstPoly, true, true);
			}
			
			this->DrawPolygonsForIndexRange<OGLPolyDrawMode_DrawTranslucentPolys>(polyList, indexList, polyList->opaqueCount, polyList->count - 1, indexOffset, lastPolyAttr);
		}
		
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);
		this->DisableVertexAttributes();
	}
	
	this->DownsampleFBO();
	
	this->_lastTextureDrawTarget = OGLTextureUnitID_GColor;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::RenderEdgeMarking(const u16 *colorTable, const bool useAntialias)
{
	if (!this->_deviceInfo.isEdgeMarkSupported)
	{
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	
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
	
	// Set up the postprocessing states
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.fboRenderAlphaID);
	
	glViewport(0, 0, this->_framebufferWidth, this->_framebufferHeight);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	
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
	
	// Pass 1: Determine the pixels with zero alpha
	glDrawBuffer(GL_NONE);
	glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT);
	glBlitFramebufferEXT(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	
	glStencilFunc(GL_ALWAYS, 0x80, 0x80);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilMask(0x80);
	
	glUseProgram(OGLRef.programGeometryZeroDstAlphaID);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
	
	// Pass 2: Unblended edge mark colors to zero-alpha pixels
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	glUseProgram(OGLRef.programEdgeMarkID);
	glUniform2f(OGLRef.uniformFramebufferSize, this->_framebufferWidth, this->_framebufferHeight);
	glUniform4fv(OGLRef.uniformStateEdgeColor, 8, oglColor);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	glStencilFunc(GL_NOTEQUAL, 0x80, 0x80);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
	
	// Pass 3: Blended edge mark
	glEnable(GL_BLEND);
	glDisable(GL_STENCIL_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(0);
	}
	else
	{
		glDisableVertexAttribArray(OGLVertexAttributeID_Position);
		glDisableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	}
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderAlphaID);
	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	this->_lastTextureDrawTarget = OGLTextureUnitID_GColor;
		
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::RenderFog(const u8 *densityTable, const u32 color, const u32 offset, const u8 shift, const bool alphaOnly)
{
	if (!this->_deviceInfo.isFogSupported)
	{
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	
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
	
	const GLfloat oglOffset = (GLfloat)(offset & 0x7FFF) / 32767.0f;
	const GLfloat oglFogStep = (GLfloat)(0x0400 >> shift) / 32767.0f;
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboPostprocessID);
	glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);
	glUseProgram(OGLRef.programFogID);
	glUniform1i(OGLRef.uniformStateEnableFogAlphaOnly, (alphaOnly) ? GL_TRUE : GL_FALSE);
	glUniform4f(OGLRef.uniformStateFogColor, oglColor[0], oglColor[1], oglColor[2], oglColor[3]);
	glUniform1f(OGLRef.uniformStateFogOffset, oglOffset);
	glUniform1f(OGLRef.uniformStateFogStep, oglFogStep);
	glUniform1fv(OGLRef.uniformStateFogDensity, 32, oglDensityTable);
	
	glViewport(0, 0, this->_framebufferWidth, this->_framebufferHeight);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	
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
	
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(0);
	}
	else
	{
		glDisableVertexAttribArray(OGLVertexAttributeID_Position);
		glDisableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	}
	
	glReadBuffer(GL_COLOR_ATTACHMENT1_EXT);
	this->_lastTextureDrawTarget = OGLTextureUnitID_FinalColor;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::EndRender(const u64 frameCount)
{
	//needs to happen before endgl because it could free some textureids for expired cache items
	texCache.Evict();
	
	this->ReadBackPixels();
	
	ENDGL();
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::UpdateToonTable(const u16 *toonTableBuffer)
{
	if (this->isShaderSupported)
	{
		glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ToonTable);
		glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 32, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, toonTableBuffer);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 *__restrict polyIDBuffer)
{
	if (!this->isFBOSupported)
	{
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	this->UploadClearImage(colorBuffer, depthBuffer, fogBuffer, polyIDBuffer);
	
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboClearImageID);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
	
	// It might seem wasteful to be doing a separate glClear(GL_STENCIL_BUFFER_BIT) instead
	// of simply blitting the stencil buffer with everything else.
	//
	// We do this because glBlitFramebufferEXT() for GL_STENCIL_BUFFER_BIT has been tested
	// to be unsupported on ATI/AMD GPUs running in compatibility mode. So we do the separate
	// glClear() for GL_STENCIL_BUFFER_BIT to keep these GPUs working.
	glClearStencil(polyIDBuffer[0]);
	glClear(GL_STENCIL_BUFFER_BIT);
	
	if (this->isShaderSupported)
	{
		// Blit the polygon ID buffer
		glReadBuffer(GL_COLOR_ATTACHMENT1_EXT);
		glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);
		glBlitFramebufferEXT(0, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GPU_FRAMEBUFFER_NATIVE_WIDTH, 0, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the fog buffer
		glReadBuffer(GL_COLOR_ATTACHMENT2_EXT);
		glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
		glBlitFramebufferEXT(0, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GPU_FRAMEBUFFER_NATIVE_WIDTH, 0, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the color buffer. Do this last so that color attachment 0 is set to the read FBO.
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glBlitFramebufferEXT(0, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GPU_FRAMEBUFFER_NATIVE_WIDTH, 0, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
		glDrawBuffers(3, RenderDrawList);
	}
	else
	{
		glBlitFramebufferEXT(0, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GPU_FRAMEBUFFER_NATIVE_WIDTH, 0, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	}
	
	if (this->isMultisampledFBOSupported)
	{
		OGLRef.selectedRenderingFBO = (this->_enableMultisampledRendering) ? OGLRef.fboMSIntermediateRenderID : OGLRef.fboRenderID;
		if (OGLRef.selectedRenderingFBO == OGLRef.fboMSIntermediateRenderID)
		{
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
			glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
			
			glClearStencil(polyIDBuffer[0]);
			glClear(GL_STENCIL_BUFFER_BIT);
			
			if (this->isShaderSupported)
			{
				// Blit the polygon ID buffer
				glReadBuffer(GL_COLOR_ATTACHMENT1_EXT);
				glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);
				glBlitFramebufferEXT(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
				
				// Blit the fog buffer
				glReadBuffer(GL_COLOR_ATTACHMENT2_EXT);
				glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
				glBlitFramebufferEXT(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
				
				// Blit the color and depth buffers. Do this last so that color attachment 0 is set to the read FBO.
				glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
				glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
				glBlitFramebufferEXT(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
				
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
				glDrawBuffers(3, RenderDrawList);
			}
			else
			{
				// Blit the color and depth buffers.
				glBlitFramebufferEXT(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
				glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
				glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
			}
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ClearUsingValues(const FragmentColor &clearColor6665, const FragmentAttributes &clearAttributes)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isFBOSupported)
	{
		OGLRef.selectedRenderingFBO = (this->_enableMultisampledRendering) ? OGLRef.fboMSIntermediateRenderID : OGLRef.fboRenderID;
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
	}
	
	if (this->isShaderSupported && this->isFBOSupported)
	{
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); // texGColorID
		glClearColor(divide6bitBy63_LUT[clearColor6665.r], divide6bitBy63_LUT[clearColor6665.g], divide6bitBy63_LUT[clearColor6665.b], divide5bitBy31_LUT[clearColor6665.a]);
		glClearDepth((GLclampd)clearAttributes.depth / (GLclampd)0x00FFFFFF);
		glClearStencil(clearAttributes.opaquePolyID);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		
		glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT); // texGPolyID
		glClearColor((GLfloat)clearAttributes.opaquePolyID/63.0f, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		
		glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT); // texGFogAttrID
		glClearColor(clearAttributes.isFogged, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		
		glDrawBuffers(3, RenderDrawList);
		
		this->_needsZeroDstAlphaPass = (clearColor6665.a == 0);
	}
	else
	{
		if (this->isFBOSupported)
		{
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		}
		
		glClearColor(divide6bitBy63_LUT[clearColor6665.r], divide6bitBy63_LUT[clearColor6665.g], divide6bitBy63_LUT[clearColor6665.b], divide5bitBy31_LUT[clearColor6665.a]);
		glClearDepth((GLclampd)clearAttributes.depth / (GLclampd)0x00FFFFFF);
		glClearStencil(clearAttributes.opaquePolyID);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::SetPolygonIndex(const size_t index)
{
	this->_currentPolyIndex = index;
}

Render3DError OpenGLRenderer_1_2::SetupPolygon(const POLY &thePoly, bool treatAsTranslucent, bool willChangeStencilBuffer)
{
	// Set up depth test mode
	glDepthFunc((thePoly.attribute.DepthEqualTest_Enable) ? GL_EQUAL : GL_LESS);
	
	// Set up culling mode
	static const GLenum oglCullingMode[4] = {GL_FRONT_AND_BACK, GL_FRONT, GL_BACK, 0};
	GLenum cullingMode = oglCullingMode[thePoly.attribute.SurfaceCullingMode];
	
	if (cullingMode == 0)
	{
		glDisable(GL_CULL_FACE);
	}
	else
	{
		glEnable(GL_CULL_FACE);
		glCullFace(cullingMode);
	}
	
	if (willChangeStencilBuffer)
	{
		// Handle drawing states for the polygon
		if (thePoly.attribute.Mode == POLYGON_MODE_SHADOW)
		{
			// Set up shadow polygon states.
			//
			// See comments in DrawShadowPolygon() for more information about
			// how this 5-pass process works in OpenGL.
			if (thePoly.attribute.PolygonID == 0)
			{
				// 1st pass: Use stencil buffer bit 7 (0x80) for the shadow volume mask.
				// Write only on depth-fail.
				glStencilFunc(GL_ALWAYS, 0x80, 0x80);
				glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);
				glStencilMask(0x80);
			}
			else
			{
				// 2nd pass: Compare stencil buffer bits 0-5 (0x3F) with this polygon's ID. If this stencil
				// test fails, remove the fragment from the shadow volume mask by clearing bit 7.
				glStencilFunc(GL_NOTEQUAL, thePoly.attribute.PolygonID, 0x3F);
				glStencilOp(GL_ZERO, GL_KEEP, GL_KEEP);
				glStencilMask(0x80);
			}
			
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glDepthMask(GL_FALSE);
		}
		else
		{
			// Polygon IDs are always written for every polygon, whether they are opaque or transparent, just as
			// long as they pass the stencil and depth tests.
			// - Polygon IDs are contained in stencil bits 0-5 (0x3F).
			// - The translucent fragment flag is contained in stencil bit 6 (0x40).
			//
			// Opaque polygons have no stencil conditions, so if they pass the depth test, then they write out
			// their polygon ID with a translucent fragment flag of 0.
			//
			// Transparent polygons have the stencil condition where they will not draw if they are drawing on
			// top of previously drawn translucent fragments with the same polygon ID. This condition is checked
			// using both polygon ID bits and the translucent fragment flag. If the polygon passes both stencil
			// and depth tests, it writes out its polygon ID with a translucent fragment flag of 1.
			if (treatAsTranslucent)
			{
				glStencilFunc(GL_NOTEQUAL, 0x40 | thePoly.attribute.PolygonID, 0x7F);
			}
			else
			{
				glStencilFunc(GL_ALWAYS, thePoly.attribute.PolygonID, 0x3F);
			}
			
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
			glStencilMask(0xFF); // Drawing non-shadow polygons will implicitly reset the shadow volume mask.
			
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDepthMask((!treatAsTranslucent || thePoly.attribute.TranslucentDepthWrite_Enable) ? GL_TRUE : GL_FALSE);
		}
	}
	
	// Set up polygon attributes
	if (this->isShaderSupported)
	{
		OGLRenderRef &OGLRef = *this->ref;
		glUniform1i(OGLRef.uniformPolyMode, thePoly.attribute.Mode);
		glUniform1i(OGLRef.uniformPolyEnableFog, (thePoly.attribute.Fog_Enable) ? GL_TRUE : GL_FALSE);
		glUniform1f(OGLRef.uniformPolyAlpha, (thePoly.isWireframe()) ? 1.0f : divide5bitBy31_LUT[thePoly.attribute.Alpha]);
		glUniform1i(OGLRef.uniformPolyID, thePoly.attribute.PolygonID);
		glUniform1i(OGLRef.uniformPolyIsWireframe, (thePoly.isWireframe()) ? GL_TRUE : GL_FALSE);
		glUniform1i(OGLRef.uniformPolySetNewDepthForTranslucent, (thePoly.attribute.TranslucentDepthWrite_Enable) ? GL_TRUE : GL_FALSE);
		glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 0);
	}
	else
	{
		// Set the texture blending mode
		static const GLint oglTexBlendMode[4] = {GL_MODULATE, GL_DECAL, GL_MODULATE, GL_MODULATE};
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, oglTexBlendMode[thePoly.attribute.Mode]);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupTexture(const POLY &thePoly, size_t polyRenderIndex)
{
	OpenGLTexture *theTexture = (OpenGLTexture *)this->_textureList[polyRenderIndex];
	const NDSTextureFormat packFormat = theTexture->GetPackFormat();
	const OGLRenderRef &OGLRef = *this->ref;
	
	// Check if we need to use textures
	if (!theTexture->IsSamplingEnabled())
	{
		if (this->isShaderSupported)
		{
			glUniform1i(OGLRef.uniformPolyEnableTexture, GL_FALSE);
			glUniform1i(OGLRef.uniformTexSingleBitAlpha, GL_FALSE);
			glUniform2f(OGLRef.uniformPolyTexScale, theTexture->GetInvWidth(), theTexture->GetInvHeight());
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
		glUniform1i(OGLRef.uniformTexSingleBitAlpha, (packFormat != TEXMODE_A3I5 && packFormat != TEXMODE_A5I3) ? GL_TRUE : GL_FALSE);
		glUniform2f(OGLRef.uniformPolyTexScale, theTexture->GetInvWidth(), theTexture->GetInvHeight());
	}
	else
	{
		glEnable(GL_TEXTURE_2D);
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glScalef(theTexture->GetInvWidth(), theTexture->GetInvHeight(), 1.0f);
	}
	
	glBindTexture(GL_TEXTURE_2D, theTexture->GetID());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ((thePoly.texParam.RepeatS_Enable) ? ((thePoly.texParam.MirroredRepeatS_Enable) ? OGLRef.stateTexMirroredRepeat : GL_REPEAT) : GL_CLAMP_TO_EDGE));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ((thePoly.texParam.RepeatT_Enable) ? ((thePoly.texParam.MirroredRepeatT_Enable) ? OGLRef.stateTexMirroredRepeat : GL_REPEAT) : GL_CLAMP_TO_EDGE));
	
	if (this->_enableTextureSmoothing)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (this->_textureScalingFactor > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, this->_deviceInfo.maxAnisotropy);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
	}
	
	theTexture->ResetCacheAge();
	theTexture->IncreaseCacheUsageCount(1);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupViewport(const u32 viewportValue)
{
	const GLfloat wScalar = this->_framebufferWidth  / (GLfloat)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const GLfloat hScalar = this->_framebufferHeight / (GLfloat)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	VIEWPORT viewport;
	viewport.decode(viewportValue);
	
	// The maximum viewport y-value is 191. Values above 191 need to wrap
	// around and go negative.
	//
	// Test case: The Homie Rollerz character select screen sets the y-value
	// to 253, which then wraps around to -2.
	glViewport( viewport.x * wScalar,
			   (viewport.y > 191) ? (viewport.y - 0xFF) * hScalar : viewport.y * hScalar,
			    viewport.width  * wScalar,
			    viewport.height * hScalar);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DrawShadowPolygon(const GLenum polyPrimitive, const GLsizei vertIndexCount, const GLushort *indexBufferPtr, const bool performDepthEqualTest, const bool enableAlphaDepthWrite, const bool isTranslucent, const u8 opaquePolyID)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Shadow polygons are actually drawn over the course of multiple passes.
	// Note that the 1st and 2nd passes are performed using states from SetupPolygon().
	//
	// 1st pass (NDS driven): The NDS creates the shadow volume and updates only the
	// stencil buffer, writing to bit 7 (0x80). Color and depth writes are disabled for this
	// pass.
	//
	// 2nd pass (NDS driven): Normally, stencil buffer bits marked for shadow rendering
	// are supposed to be drawn in this step, but there are additional checks that need to
	// be made before writing out the fragment. Since OpenGL can only do one type of stencil
	// buffer check at a time, we need to do things differently from what the NDS does at
	// this point.
	//
	// In OpenGL, this pass is used only to update the stencil buffer for the polygon
	// ID check, checking bits 0x3F for the polygon ID, and clearing bit 7 (0x80) if this
	// check fails. Color and depth writes are disabled
	//
	// 3rd pass (emulator driven): This pass only occurs when the shadow polygon is
	// transparent, which is the typical case. Since transparent polygons have a rule for
	// which they cannot draw fragments on top of previously drawn translucent fragments with
	// the same polygon IDs, we also need to do an additional polygon ID check to ensure that
	// it isn't a transparent polygon ID. We continue to check bits 0x3F for the polygon ID,
	// in addition to also checking the translucent fragment flag at bit 6 (0x40). If this
	// check fails, then bit 7 (0x80) is cleared. Color and depth writes are disabled for this
	// pass.
	//
	// 4th pass (emulator driven): Use stencil buffer bit 7 (0x70) for the shadow volume
	// mask and write out the polygon ID and translucent fragment flag only to those fragments
	// within the mask. Color and depth writes are disabled for this pass.
	//
	// 5th pass (emulator driven): Use stencil buffer bit 7 (0x70) for the shadow volume
	// mask and draw the shadow polygon fragments only within the mask. Color writes are always
	// enabled and depth writes are enabled if the shadow polygon is opaque or if transparent
	// polygon depth writes are enabled.
	
	// 1st pass: Create the shadow volume.
	if (opaquePolyID == 0)
	{
		if (performDepthEqualTest && this->isShaderSupported)
		{
			// Use the stencil buffer to determine which fragments fail the depth test using the lower-side tolerance.
			glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 1);
			glDepthFunc(GL_LEQUAL);
			glStencilFunc(GL_ALWAYS, 0x80, 0x80);
			glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);
			glStencilMask(0x80);
			glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
			
			// Use the stencil buffer to determine which fragments fail the depth test using the higher-side tolerance.
			glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 2);
			glDepthFunc(GL_GEQUAL);
			glStencilFunc(GL_NOTEQUAL, 0x80, 0x80);
			glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);
			glStencilMask(0x80);
			glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		}
		else
		{
			glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		}
		
		return OGLERROR_NOERR;
	}
	
	// 2nd pass: Do the polygon ID check.
	if (performDepthEqualTest && this->isShaderSupported)
	{
		// Use the stencil buffer to determine which fragments pass the lower-side tolerance.
		glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 1);
		glDepthFunc(GL_LEQUAL);
		glStencilFunc(GL_EQUAL, 0x80, 0x80);
		glStencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
		glStencilMask(0x80);
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		
		// Use the stencil buffer to determine which fragments pass the higher-side tolerance.
		glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 2);
		glDepthFunc(GL_GEQUAL);
		glStencilFunc(GL_EQUAL, 0x80, 0x80);
		glStencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
		glStencilMask(0x80);
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		
		// Check both the depth-equals test bit (bit 7) and shadow volume mask bit (bit 6).
		// Fragments that fail this stencil test are removed from the shadow volume mask.
		glUniform1i(OGLRef.uniformPolyDepthOffsetMode, 0);
		glDepthFunc(GL_ALWAYS);
		glStencilFunc(GL_NOTEQUAL, opaquePolyID, 0x3F);
		glStencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
		glStencilMask(0x80);
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
	}
	else
	{
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
	}
	
	// 3rd pass: Do the transparent polygon ID check. For transparent shadow polygons, we need to
	// also ensure that we're not drawing over translucent fragments with the same polygon IDs.
	if (isTranslucent)
	{
		glStencilFunc(GL_NOTEQUAL, 0xC0 | opaquePolyID, 0x7F);
		glStencilOp(GL_ZERO, GL_KEEP, GL_KEEP);
		glStencilMask(0x80);
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
	}
	
	// 4th pass: Update the polygon IDs in the stencil buffer.
	glStencilFunc(GL_EQUAL, (isTranslucent) ? 0xC0 | opaquePolyID : 0x80 | opaquePolyID, 0x80);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilMask(0x7F);
	glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
	
	// 5th pass: Draw the shadow polygon.
	glStencilFunc(GL_EQUAL, 0x80, 0x80);
	// Technically, a depth-fail result should also clear the shadow volume mask, but
	// Mario Kart DS draws shadow polygons better when it doesn't clear bits on depth-fail.
	// I have no idea why this works. - rogerman 2016/12/21
	glStencilOp(GL_ZERO, GL_KEEP, GL_ZERO);
	glStencilMask(0x80);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask((!isTranslucent || enableAlphaDepthWrite) ? GL_TRUE : GL_FALSE);
	
	if (this->isShaderSupported)
	{
		glUniform1i(OGLRef.uniformPolyDrawShadow, GL_TRUE);
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		glUniform1i(OGLRef.uniformPolyDrawShadow, GL_FALSE);
	}
	else
	{
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
	}
	
	// Reset the OpenGL states back to their original shadow polygon states.
	glStencilFunc(GL_NOTEQUAL, opaquePolyID, 0x3F);
	glStencilOp(GL_ZERO, GL_KEEP, GL_KEEP);
	glStencilMask(0x80);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_FALSE);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::Reset()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if(!BEGINGL())
	{
		return OGLERROR_BEGINGL_FAILED;
	}
	
	glFinish();
	
	if (!this->isShaderSupported)
	{
		glEnable(GL_NORMALIZE);
		glEnable(GL_TEXTURE_1D);
		glEnable(GL_TEXTURE_2D);
		glAlphaFunc(GL_GREATER, 0);
		glEnable(GL_ALPHA_TEST);
		glEnable(GL_BLEND);
	}
	
	ENDGL();
	
	this->_pixelReadNeedsFinish = false;
	
	if (OGLRef.color4fBuffer != NULL)
	{
		memset(OGLRef.color4fBuffer, 0, VERTLIST_SIZE * 4 * sizeof(GLfloat));
	}
	
	if (OGLRef.vertIndexBuffer != NULL)
	{
		memset(OGLRef.vertIndexBuffer, 0, OGLRENDER_VERT_INDEX_BUFFER_COUNT * sizeof(GLushort));
	}
	
	this->_currentPolyIndex = 0;
	
	OGLRef.vtxPtrPosition = (GLvoid *)offsetof(VERT, coord);
	OGLRef.vtxPtrTexCoord = (GLvoid *)offsetof(VERT, texcoord);
	OGLRef.vtxPtrColor = (this->isShaderSupported) ? (GLvoid *)offsetof(VERT, color) : OGLRef.color4fBuffer;
	
	memset(this->clearImageColor16Buffer, 0, sizeof(this->clearImageColor16Buffer));
	memset(this->clearImageDepthBuffer, 0, sizeof(this->clearImageDepthBuffer));
	memset(this->clearImagePolyIDBuffer, 0, sizeof(this->clearImagePolyIDBuffer));
	memset(this->clearImageFogBuffer, 0, sizeof(this->clearImageFogBuffer));
	
	texCache.Reset();
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::RenderFinish()
{
	if (!this->_renderNeedsFinish)
	{
		return OGLERROR_NOERR;
	}
	
	if (this->_pixelReadNeedsFinish)
	{
		this->_pixelReadNeedsFinish = false;
		
		if(!BEGINGL())
		{
			return OGLERROR_BEGINGL_FAILED;
		}
		
		if (this->isPBOSupported)
		{
			this->_mappedFramebuffer = (FragmentColor *__restrict)glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
		}
		else
		{
			glReadPixels(0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_BGRA, GL_UNSIGNED_BYTE, this->_framebufferColor);
		}
		
		ENDGL();
	}
	
	this->_renderNeedsFlushMain = true;
	this->_renderNeedsFlush16 = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::RenderFlush(bool willFlushBuffer32, bool willFlushBuffer16)
{
	FragmentColor *framebufferMain = (willFlushBuffer32) ? GPU->GetEngineMain()->Get3DFramebufferMain() : NULL;
	u16 *framebuffer16 = (willFlushBuffer16) ? GPU->GetEngineMain()->Get3DFramebuffer16() : NULL;
	
	if (this->isPBOSupported)
	{
		this->FlushFramebuffer(this->_mappedFramebuffer, framebufferMain, framebuffer16);
	}
	else
	{
		this->FlushFramebuffer(this->_framebufferColor, framebufferMain, framebuffer16);
	}
	
	return RENDER3DERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetFramebufferSize(size_t w, size_t h)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (w < GPU_FRAMEBUFFER_NATIVE_WIDTH || h < GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		return OGLERROR_NOERR;
	}
	
	if (!BEGINGL())
	{
		return OGLERROR_BEGINGL_FAILED;
	}
	
	const size_t newFramebufferColorSizeBytes = w * h * sizeof(FragmentColor);
	
	if (this->isPBOSupported)
	{
		if (this->_mappedFramebuffer != NULL)
		{
			glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
		}
		
		glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, newFramebufferColorSizeBytes, NULL, GL_STREAM_READ_ARB);
		this->_mappedFramebuffer = (FragmentColor *__restrict)glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
	}
	
	if (this->isShaderSupported && this->isFBOSupported && this->isVBOSupported)
	{
		glActiveTextureARB(GL_TEXTURE0_ARB);
		glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthStencilAlphaID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, w, h, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
	}
	
	if (this->isShaderSupported || this->isFBOSupported)
	{
		glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_FinalColor);
		glBindTexture(GL_TEXTURE_2D, OGLRef.texFinalColorID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	}
	
	if (this->isFBOSupported)
	{
		glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_GColor);
		glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthStencilID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, w, h, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
		glBindTexture(GL_TEXTURE_2D, OGLRef.texGColorID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
		
		glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_GPolyID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
		
		glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ZeroAlphaPixelMask);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
		
		glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_FogAttr);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	}
	
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
	if (this->isMultisampledFBOSupported)
	{
		GLsizei sampleSize = this->GetLimitedMultisampleSize();
		
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGColorID);
		glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleSize, GL_RGBA, w, h);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGPolyID);
		glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleSize, GL_RGBA, w, h);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGFogAttrID);
		glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleSize, GL_RGBA, w, h);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilID);
		glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleSize, GL_DEPTH24_STENCIL8_EXT, w, h);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilAlphaID);
		glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleSize, GL_DEPTH24_STENCIL8_EXT, w, h);
	}
	
	this->_framebufferWidth = w;
	this->_framebufferHeight = h;
	this->_framebufferColorSizeBytes = newFramebufferColorSizeBytes;
	
	if (this->isPBOSupported)
	{
		this->_framebufferColor = NULL;
	}
	else
	{
		FragmentColor *oldFramebufferColor = this->_framebufferColor;
		FragmentColor *newFramebufferColor = (FragmentColor *)malloc_alignedCacheLine(newFramebufferColorSizeBytes);
		this->_framebufferColor = newFramebufferColor;
		free_aligned(oldFramebufferColor);
	}
	
	if (oglrender_framebufferDidResizeCallback != NULL)
	{
		oglrender_framebufferDidResizeCallback(w, h);
	}
	
	ENDGL();
	
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
	
	// Ignore our color buffer since we'll transfer the polygon alpha through a uniform.
	OGLRef.color4fBuffer = NULL;
	
	// VBOs are supported here, so just use the index buffer on the GPU.
	OGLRef.vertIndexBuffer = NULL;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::EnableVertexAttributes()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(OGLRef.vaoGeometryStatesID);
	}
	else
	{
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
		glEnableVertexAttribArray(OGLVertexAttributeID_Color);
		glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), OGLRef.vtxPtrPosition);
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), OGLRef.vtxPtrTexCoord);
		glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), OGLRef.vtxPtrColor);
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
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::BeginRender(const GFX3D &engine)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if(!BEGINGL())
	{
		return OGLERROR_BEGINGL_FAILED;
	}
	
	// Setup render states
	glUseProgram(OGLRef.programGeometryID);
	glUniform1i(OGLRef.uniformStateToonShadingMode, engine.renderState.shading);
	glUniform1i(OGLRef.uniformStateEnableAlphaTest, (engine.renderState.enableAlphaTest) ? GL_TRUE : GL_FALSE);
	glUniform1i(OGLRef.uniformStateEnableAntialiasing, (engine.renderState.enableAntialiasing) ? GL_TRUE : GL_FALSE);
	glUniform1i(OGLRef.uniformStateEnableEdgeMarking, (this->_enableEdgeMark) ? GL_TRUE : GL_FALSE);
	glUniform1i(OGLRef.uniformStateUseWDepth, (engine.renderState.wbuffer) ? GL_TRUE : GL_FALSE);
	glUniform1f(OGLRef.uniformStateAlphaTestRef, divide5bitBy31_LUT[engine.renderState.alphaTestRef]);
	glUniform1i(OGLRef.uniformTexDrawOpaque, GL_FALSE);
	glUniform1i(OGLRef.uniformPolyDrawShadow, GL_FALSE);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
	
	size_t vertIndexCount = 0;
	GLushort *indexPtr = (GLushort *)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
	
	for (size_t i = 0; i < engine.polylist->count; i++)
	{
		const POLY *thePoly = &engine.polylist->list[engine.indexlist.list[i]];
		const size_t polyType = thePoly->type;
		
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
		
		this->_textureList[i] = this->GetLoadedTextureFromPolygon(*thePoly, this->_enableTextureSampling);
	}
	
	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * engine.vertListCount, engine.vertList);
	
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	
	this->_needsZeroDstAlphaPass = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::SetupTexture(const POLY &thePoly, size_t polyRenderIndex)
{
	OpenGLTexture *theTexture = (OpenGLTexture *)this->_textureList[polyRenderIndex];
	const NDSTextureFormat packFormat = theTexture->GetPackFormat();
	const OGLRenderRef &OGLRef = *this->ref;
	
	glUniform2f(OGLRef.uniformPolyTexScale, theTexture->GetInvWidth(), theTexture->GetInvHeight());
	
	// Check if we need to use textures
	if (!theTexture->IsSamplingEnabled())
	{
		glUniform1i(OGLRef.uniformPolyEnableTexture, GL_FALSE);
		glUniform1i(OGLRef.uniformTexSingleBitAlpha, GL_FALSE);
		return OGLERROR_NOERR;
	}
	
	glUniform1i(OGLRef.uniformPolyEnableTexture, GL_TRUE);
	glUniform1i(OGLRef.uniformTexSingleBitAlpha, (packFormat != TEXMODE_A3I5 && packFormat != TEXMODE_A5I3) ? GL_TRUE : GL_FALSE);
	
	glBindTexture(GL_TEXTURE_2D, theTexture->GetID());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ((thePoly.texParam.RepeatS_Enable) ? ((thePoly.texParam.MirroredRepeatS_Enable) ? GL_MIRRORED_REPEAT : GL_REPEAT) : GL_CLAMP_TO_EDGE));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ((thePoly.texParam.RepeatT_Enable) ? ((thePoly.texParam.MirroredRepeatT_Enable) ? GL_MIRRORED_REPEAT : GL_REPEAT) : GL_CLAMP_TO_EDGE));
	
	if (this->_enableTextureSmoothing)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (this->_textureScalingFactor > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, this->_deviceInfo.maxAnisotropy);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
	}
	
	theTexture->ResetCacheAge();
	theTexture->IncreaseCacheUsageCount(1);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_1::RenderFinish()
{
	if (!this->_renderNeedsFinish)
	{
		return OGLERROR_NOERR;
	}
	
	if (this->_pixelReadNeedsFinish)
	{
		this->_pixelReadNeedsFinish = false;
		
		if(!BEGINGL())
		{
			return OGLERROR_BEGINGL_FAILED;
		}
		
		this->_mappedFramebuffer = (FragmentColor *__restrict)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		
		ENDGL();
	}
	
	this->_renderNeedsFlushMain = true;
	this->_renderNeedsFlush16 = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_1::RenderFlush(bool willFlushBuffer32, bool willFlushBuffer16)
{
	FragmentColor *framebufferMain = (willFlushBuffer32) ? GPU->GetEngineMain()->Get3DFramebufferMain() : NULL;
	u16 *framebuffer16 = (willFlushBuffer16) ? GPU->GetEngineMain()->Get3DFramebuffer16() : NULL;
	
	this->FlushFramebuffer(this->_mappedFramebuffer, framebufferMain, framebuffer16);
	
	return RENDER3DERROR_NOERR;
}

template size_t OpenGLRenderer::DrawPolygonsForIndexRange<OGLPolyDrawMode_DrawOpaquePolys>(const POLYLIST *polyList, const INDEXLIST *indexList, size_t firstIndex, size_t lastIndex, size_t &indexOffset, POLYGON_ATTR &lastPolyAttr);
template size_t OpenGLRenderer::DrawPolygonsForIndexRange<OGLPolyDrawMode_DrawTranslucentPolys>(const POLYLIST *polyList, const INDEXLIST *indexList, size_t firstIndex, size_t lastIndex, size_t &indexOffset, POLYGON_ATTR &lastPolyAttr);
template size_t OpenGLRenderer::DrawPolygonsForIndexRange<OGLPolyDrawMode_ZeroAlphaPass>(const POLYLIST *polyList, const INDEXLIST *indexList, size_t firstIndex, size_t lastIndex, size_t &indexOffset, POLYGON_ATTR &lastPolyAttr);
