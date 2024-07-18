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

#include "OGLRender.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <sstream>

#include "common.h"
#include "debug.h"
#include "NDSSystem.h"

#include "./filter/filter.h"
#include "./filter/xbrz.h"

#ifdef ENABLE_SSE2
#include <emmintrin.h>
#include "./utils/colorspacehandler/colorspacehandler_SSE2.h"
#endif

#if defined(OPENGL_VARIANT_STANDARD)
	#if MSB_FIRST
		#define OGL_TEXTURE_SRC_CI_COLOR   GL_UNSIGNED_SHORT_1_5_5_5_REV
		#define OGL_TEXTURE_SRC_CI_FOG     GL_UNSIGNED_INT_8_8_8_8_REV
		#define OGL_TEXTURE_SRC_EDGE_COLOR GL_UNSIGNED_INT_8_8_8_8
		#define OGL_TEXTURE_SRC_TOON_TABLE GL_UNSIGNED_SHORT_1_5_5_5_REV
	#else
		#define OGL_TEXTURE_SRC_CI_COLOR   GL_UNSIGNED_SHORT_1_5_5_5_REV
		#define OGL_TEXTURE_SRC_CI_FOG     GL_UNSIGNED_INT_8_8_8_8_REV
		#define OGL_TEXTURE_SRC_EDGE_COLOR GL_UNSIGNED_INT_8_8_8_8_REV
		#define OGL_TEXTURE_SRC_TOON_TABLE GL_UNSIGNED_SHORT_1_5_5_5_REV
	#endif
#elif defined(OPENGL_VARIANT_ES)
	#define OGL_TEXTURE_SRC_CI_COLOR   GL_UNSIGNED_BYTE
	#define OGL_TEXTURE_SRC_CI_FOG     GL_UNSIGNED_BYTE
	#define OGL_TEXTURE_SRC_EDGE_COLOR GL_UNSIGNED_BYTE
	#define OGL_TEXTURE_SRC_TOON_TABLE GL_UNSIGNED_BYTE
#else
	#error Unknown OpenGL variant.
#endif

typedef struct
{
	unsigned int major;
	unsigned int minor;
	unsigned int revision;
} OGLVersion;

static OGLVersion _OGLDriverVersion = {0, 0, 0};

// Lookup Tables
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

const GLfloat PostprocessVtxBuffer[16]	= {-1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f,  1.0f,
                                            0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f};

const GLenum GeometryDrawBuffersEnum[8][4] = {
	{ OGL_COLOROUT_ATTACHMENT_ID, GL_NONE, GL_NONE, GL_NONE },
	{ OGL_COLOROUT_ATTACHMENT_ID, OGL_FOGATTRIBUTES_ATTACHMENT_ID, GL_NONE, GL_NONE },
	{ OGL_COLOROUT_ATTACHMENT_ID, OGL_POLYID_ATTACHMENT_ID, GL_NONE, GL_NONE },
	{ OGL_COLOROUT_ATTACHMENT_ID, OGL_POLYID_ATTACHMENT_ID, OGL_FOGATTRIBUTES_ATTACHMENT_ID, GL_NONE },
	{ OGL_COLOROUT_ATTACHMENT_ID, OGL_WORKING_ATTACHMENT_ID, GL_NONE, GL_NONE },
	{ OGL_COLOROUT_ATTACHMENT_ID, OGL_WORKING_ATTACHMENT_ID, OGL_FOGATTRIBUTES_ATTACHMENT_ID, GL_NONE },
	{ OGL_COLOROUT_ATTACHMENT_ID, OGL_WORKING_ATTACHMENT_ID, OGL_POLYID_ATTACHMENT_ID, GL_NONE },
	{ OGL_COLOROUT_ATTACHMENT_ID, OGL_WORKING_ATTACHMENT_ID, OGL_POLYID_ATTACHMENT_ID, OGL_FOGATTRIBUTES_ATTACHMENT_ID }
};

const GLint GeometryAttachmentWorkingBuffer[8] = {
	0,
	0,
	0,
	0,
	1,
	1,
	1,
	1
};

const GLint GeometryAttachmentPolyID[8] = {
	0,
	0,
	1,
	1,
	0,
	0,
	2,
	2
};

const GLint GeometryAttachmentFogAttributes[8] = {
	0,
	1,
	0,
	2,
	0,
	2,
	0,
	3
};

bool BEGINGL()
{
	if (oglrender_beginOpenGL)
		return oglrender_beginOpenGL();
	else return true;
}

void ENDGL()
{
	if (oglrender_endOpenGL)
		oglrender_endOpenGL();
}

// OPENGL CLIENT CONTEXT FUNCTION POINTERS
bool (*oglrender_init)() = NULL;
bool (*oglrender_beginOpenGL)() = NULL;
void (*oglrender_endOpenGL)() = NULL;
bool (*oglrender_framebufferDidResizeCallback)(const bool isFBOSupported, size_t w, size_t h) = NULL;

// OPENGL RENDERER OBJECT CREATION FUNCTION POINTERS
void (*OGLLoadEntryPoints_3_2_Func)() = NULL;
void (*OGLCreateRenderer_3_2_Func)(OpenGLRenderer **rendererPtr) = NULL;
void (*OGLLoadEntryPoints_ES_3_0_Func)() = NULL;
void (*OGLCreateRenderer_ES_3_0_Func)(OpenGLRenderer **rendererPtr) = NULL;

//------------------------------------------------------------

// Textures
#if !defined(GLX_H)
OGLEXT(PFNGLACTIVETEXTUREPROC, glActiveTexture) // Core in v1.3
#endif

// Blending
#if !defined(GLX_H)
OGLEXT(PFNGLBLENDEQUATIONPROC, glBlendEquation) // Core in v1.2
#endif
OGLEXT(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate) // Core in v1.4
OGLEXT(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate) // Core in v2.0

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

// Generic vertex attributes
OGLEXT(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) // Core in v2.0
OGLEXT(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) // Core in v2.0
OGLEXT(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) // Core in v2.0
OGLEXT(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) // Core in v2.0

// VAO (always available in Apple's implementation of OpenGL, including old versions)
OGLEXT(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) // Core in v3.0 and ES v3.0
OGLEXT(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays) // Core in v3.0 and ES v3.0
OGLEXT(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) // Core in v3.0 and ES v3.0

OGLEXT(PFNGLGENBUFFERSPROC, glGenBuffers) // Core in v1.5
OGLEXT(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) // Core in v1.5
OGLEXT(PFNGLBINDBUFFERPROC, glBindBuffer) // Core in v1.5
OGLEXT(PFNGLBUFFERDATAPROC, glBufferData) // Core in v1.5
OGLEXT(PFNGLBUFFERSUBDATAPROC, glBufferSubData) // Core in v1.5
#if defined(GL_VERSION_1_5)
OGLEXT(PFNGLMAPBUFFERPROC, glMapBuffer) // Core in v1.5
#endif
OGLEXT(PFNGLUNMAPBUFFERPROC, glUnmapBuffer) // Core in v1.5

#ifdef GL_EXT_framebuffer_object
// FBO
OGLEXT(PFNGLGENFRAMEBUFFERSEXTPROC, glGenFramebuffersEXT)
OGLEXT(PFNGLBINDFRAMEBUFFEREXTPROC, glBindFramebufferEXT)
OGLEXT(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC, glFramebufferRenderbufferEXT)
OGLEXT(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC, glFramebufferTexture2DEXT)
OGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, glCheckFramebufferStatusEXT)
OGLEXT(PFNGLDELETEFRAMEBUFFERSEXTPROC, glDeleteFramebuffersEXT)
OGLEXT(PFNGLBLITFRAMEBUFFEREXTPROC, glBlitFramebufferEXT)

// Multisampled FBO
OGLEXT(PFNGLGENRENDERBUFFERSEXTPROC, glGenRenderbuffersEXT)
OGLEXT(PFNGLBINDRENDERBUFFEREXTPROC, glBindRenderbufferEXT)
OGLEXT(PFNGLRENDERBUFFERSTORAGEEXTPROC, glRenderbufferStorageEXT)
OGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC, glRenderbufferStorageMultisampleEXT)
OGLEXT(PFNGLDELETERENDERBUFFERSEXTPROC, glDeleteRenderbuffersEXT)
#endif // GL_EXT_framebuffer_object

static void OGLLoadEntryPoints_Legacy()
{
	// Textures
#if !defined(GLX_H)
	INITOGLEXT(PFNGLACTIVETEXTUREPROC, glActiveTexture) // Core in v1.3
#endif

	// Blending
#if !defined(GLX_H)
	INITOGLEXT(PFNGLBLENDEQUATIONPROC, glBlendEquation) // Core in v1.2
#endif
	INITOGLEXT(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate) // Core in v1.4
	INITOGLEXT(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate) // Core in v2.0

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
	
	// Generic vertex attributes
	INITOGLEXT(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) // Core in v2.0
	INITOGLEXT(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) // Core in v2.0
	INITOGLEXT(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) // Core in v2.0
	INITOGLEXT(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) // Core in v2.0

	// Buffer Objects
	INITOGLEXT(PFNGLGENBUFFERSPROC, glGenBuffers) // Core in v1.5
	INITOGLEXT(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) // Core in v1.5
	INITOGLEXT(PFNGLBINDBUFFERPROC, glBindBuffer) // Core in v1.5
	INITOGLEXT(PFNGLBUFFERDATAPROC, glBufferData) // Core in v1.5
	INITOGLEXT(PFNGLBUFFERSUBDATAPROC, glBufferSubData) // Core in v1.5
#if defined(GL_VERSION_1_5)
	INITOGLEXT(PFNGLMAPBUFFERPROC, glMapBuffer) // Core in v1.5
#endif
	INITOGLEXT(PFNGLUNMAPBUFFERPROC, glUnmapBuffer) // Core in v1.5
	
	// VAO (always available in Apple's implementation of OpenGL, including old versions)
	INITOGLEXT(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) // Core in v3.0 and ES v3.0
	INITOGLEXT(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays) // Core in v3.0 and ES v3.0
	INITOGLEXT(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) // Core in v3.0 and ES v3.0

#ifdef GL_EXT_framebuffer_object
	// FBO
	INITOGLEXT(PFNGLGENFRAMEBUFFERSEXTPROC, glGenFramebuffersEXT)
	INITOGLEXT(PFNGLBINDFRAMEBUFFEREXTPROC, glBindFramebufferEXT)
	INITOGLEXT(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC, glFramebufferRenderbufferEXT)
	INITOGLEXT(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC, glFramebufferTexture2DEXT)
	INITOGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, glCheckFramebufferStatusEXT)
	INITOGLEXT(PFNGLDELETEFRAMEBUFFERSEXTPROC, glDeleteFramebuffersEXT)
	INITOGLEXT(PFNGLBLITFRAMEBUFFEREXTPROC, glBlitFramebufferEXT)
	
	// Multisampled FBO
	INITOGLEXT(PFNGLGENRENDERBUFFERSEXTPROC, glGenRenderbuffersEXT)
	INITOGLEXT(PFNGLBINDRENDERBUFFEREXTPROC, glBindRenderbufferEXT)
	INITOGLEXT(PFNGLRENDERBUFFERSTORAGEEXTPROC, glRenderbufferStorageEXT)
	INITOGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC, glRenderbufferStorageMultisampleEXT)
	INITOGLEXT(PFNGLDELETERENDERBUFFERSEXTPROC, glDeleteRenderbuffersEXT)
#endif // GL_EXT_framebuffer_object
}

// Vertex Shader GLSL 1.00
static const char *GeometryVtxShader_100 = {"\
attribute vec4 inPosition; \n\
attribute vec2 inTexCoord0; \n\
attribute vec3 inColor; \n\
\n\
uniform int polyMode;\n\
uniform bool polyDrawShadow;\n\
\n\
uniform float polyAlpha; \n\
uniform vec2 polyTexScale; \n\
\n\
varying vec2 vtxTexCoord; \n\
varying vec4 vtxColor; \n\
varying float isPolyDrawable;\n\
\n\
void main() \n\
{ \n\
	mat2 texScaleMtx	= mat2(	vec2(polyTexScale.x,            0.0), \n\
								vec2(           0.0, polyTexScale.y)); \n\
	\n\
	vtxTexCoord = (texScaleMtx * inTexCoord0) / 16.0; \n\
	vtxColor = vec4(inColor / 63.0, polyAlpha); \n\
	isPolyDrawable = ((polyMode != 3) || polyDrawShadow) ? 1.0 : -1.0;\n\
	\n\
	gl_Position = inPosition / 4096.0;\n\
} \n\
"};

// Fragment Shader GLSL 1.00
static const char *GeometryFragShader_100 = {"\
varying vec2 vtxTexCoord;\n\
varying vec4 vtxColor;\n\
varying float isPolyDrawable;\n\
\n\
uniform sampler2D texRenderObject;\n\
uniform sampler1D texToonTable;\n\
\n\
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
uniform bool polyIsBackFacing;\n\
\n\
uniform bool drawModeDepthEqualsTest;\n\
uniform bool polyDrawShadow;\n\
uniform float polyDepthOffset;\n\
\n\
#if USE_DEPTH_LEQUAL_POLYGON_FACING && !DRAW_MODE_OPAQUE\n\
uniform sampler2D inDstBackFacing;\n\
#endif\n\
\n\
void main()\n\
{\n\
#if USE_DEPTH_LEQUAL_POLYGON_FACING && !DRAW_MODE_OPAQUE\n\
	bool isOpaqueDstBackFacing = bool( texture2D(inDstBackFacing, vec2(gl_FragCoord.x/FRAMEBUFFER_SIZE_X, gl_FragCoord.y/FRAMEBUFFER_SIZE_Y)).r );\n\
	if (drawModeDepthEqualsTest && (polyIsBackFacing || !isOpaqueDstBackFacing))\n\
	{\n\
		discard;\n\
	}\n\
#endif\n\
	\n\
	vec4 mainTexColor = (ENABLE_TEXTURE_SAMPLING && polyEnableTexture) ? texture2D(texRenderObject, vtxTexCoord) : vec4(1.0, 1.0, 1.0, 1.0);\n\
	vec3 newToonColor = texture1D(texToonTable, vtxColor.r).rgb;\n\
	\n\
	if (!texSingleBitAlpha)\n\
	{\n\
		if (texDrawOpaque && (polyMode != 1) && (mainTexColor.a <= 0.999))\n\
		{\n\
			discard;\n\
		}\n\
	}\n\
#if USE_TEXTURE_SMOOTHING\n\
	else\n\
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
#endif\n\
	\n\
	vec4 newFragColor = mainTexColor * vtxColor;\n\
	\n\
	if (polyMode == 1)\n\
	{\n\
		newFragColor.rgb = (ENABLE_TEXTURE_SAMPLING && polyEnableTexture) ? mix(vtxColor.rgb, mainTexColor.rgb, mainTexColor.a) : vtxColor.rgb;\n\
		newFragColor.a = vtxColor.a;\n\
	}\n\
	else if (polyMode == 2)\n\
	{\n\
#if TOON_SHADING_MODE\n\
		newFragColor.rgb = min((mainTexColor.rgb * vtxColor.r) + newToonColor.rgb, 1.0);\n\
#else\n\
		newFragColor.rgb = mainTexColor.rgb * newToonColor.rgb;\n\
#endif\n\
	}\n\
	else if ((polyMode == 3) && polyDrawShadow)\n\
	{\n\
		newFragColor = vtxColor;\n\
	}\n\
	\n\
	if ( (isPolyDrawable > 0.0) && ((newFragColor.a < 0.001) || (ENABLE_ALPHA_TEST && (newFragColor.a < stateAlphaTestRef))) )\n\
	{\n\
		discard;\n\
	}\n\
	\n\
	OUTFRAGCOLOR = newFragColor;\n\
	\n\
#if ENABLE_EDGE_MARK\n\
	gl_FragData[ATTACHMENT_POLY_ID] = (isPolyDrawable > 0.0) ? vec4( float(polyID)/63.0, float(polyIsWireframe), 0.0, float(newFragColor.a > 0.999) ) : vec4(0.0, 0.0, 0.0, 0.0);\n\
#endif\n\
#if ENABLE_FOG\n\
	gl_FragData[ATTACHMENT_FOG_ATTRIBUTES] = (isPolyDrawable > 0.0) ? vec4( float(polyEnableFog), 0.0, 0.0, float((newFragColor.a > 0.999) ? 1.0 : 0.5) ) : vec4(0.0, 0.0, 0.0, 0.0);\n\
#endif\n\
#if DRAW_MODE_OPAQUE\n\
	gl_FragData[ATTACHMENT_WORKING_BUFFER] = vec4(float(polyIsBackFacing), 0.0, 0.0, 1.0);\n\
#endif\n\
	\n\
#if USE_NDS_DEPTH_CALCULATION || ENABLE_FOG\n\
	// It is tempting to perform the NDS depth calculation in the vertex shader rather than in the fragment shader.\n\
	// Resist this temptation! It is much more reliable to do the depth calculation in the fragment shader due to\n\
	// subtle interpolation differences between various GPUs and/or drivers. If the depth calculation is not done\n\
	// here, then it is very possible for the user to experience Z-fighting in certain rendering situations.\n\
	\n\
	#if ENABLE_W_DEPTH\n\
	gl_FragDepth = clamp( ((1.0/gl_FragCoord.w) * (4096.0/16777215.0)) + polyDepthOffset, 0.0, 1.0 );\n\
	#else\n\
	// hack: when using z-depth, drop some LSBs so that the overworld map in Dragon Quest IV shows up correctly\n\
	gl_FragDepth = clamp( (floor(gl_FragCoord.z * 4194303.0) * (4.0/16777215.0)) + polyDepthOffset, 0.0, 1.0 );\n\
	#endif\n\
#endif\n\
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
varying vec2 texCoord[5];\n\
\n\
void main()\n\
{\n\
	vec2 texInvScale = vec2(1.0/FRAMEBUFFER_SIZE_X, 1.0/FRAMEBUFFER_SIZE_Y);\n\
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
uniform sampler1D texEdgeColor;\n\
\n\
uniform int clearPolyID;\n\
uniform float clearDepth;\n\
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
	vec4 edgeColor[5];\n\
	edgeColor[0] = texture1D(texEdgeColor, polyIDInfo[0].r);\n\
	edgeColor[1] = texture1D(texEdgeColor, polyIDInfo[1].r);\n\
	edgeColor[2] = texture1D(texEdgeColor, polyIDInfo[2].r);\n\
	edgeColor[3] = texture1D(texEdgeColor, polyIDInfo[3].r);\n\
	edgeColor[4] = texture1D(texEdgeColor, polyIDInfo[4].r);\n\
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
		bool isEdgeMarkingClearValues = ((polyID[0] != clearPolyID) && (depth[0] < clearDepth) && !isWireframe[0]);\n\
		\n\
		if ( ((gl_FragCoord.x >= FRAMEBUFFER_SIZE_X-1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[1]) && (depth[0] >= depth[1]) && !isWireframe[1])) )\n\
		{\n\
			if (gl_FragCoord.x >= FRAMEBUFFER_SIZE_X-1.0)\n\
			{\n\
				newEdgeColor = edgeColor[0];\n\
			}\n\
			else\n\
			{\n\
				newEdgeColor = edgeColor[1];\n\
			}\n\
		}\n\
		else if ( ((gl_FragCoord.y >= FRAMEBUFFER_SIZE_Y-1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[2]) && (depth[0] >= depth[2]) && !isWireframe[2])) )\n\
		{\n\
			if (gl_FragCoord.y >= FRAMEBUFFER_SIZE_Y-1.0)\n\
			{\n\
				newEdgeColor = edgeColor[0];\n\
			}\n\
			else\n\
			{\n\
				newEdgeColor = edgeColor[2];\n\
			}\n\
		}\n\
		else if ( ((gl_FragCoord.x < 1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[3]) && (depth[0] >= depth[3]) && !isWireframe[3])) )\n\
		{\n\
			if (gl_FragCoord.x < 1.0)\n\
			{\n\
				newEdgeColor = edgeColor[0];\n\
			}\n\
			else\n\
			{\n\
				newEdgeColor = edgeColor[3];\n\
			}\n\
		}\n\
		else if ( ((gl_FragCoord.y < 1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[4]) && (depth[0] >= depth[4]) && !isWireframe[4])) )\n\
		{\n\
			if (gl_FragCoord.y < 1.0)\n\
			{\n\
				newEdgeColor = edgeColor[0];\n\
			}\n\
			else\n\
			{\n\
				newEdgeColor = edgeColor[4];\n\
			}\n\
		}\n\
	}\n\
	\n\
	gl_FragColor = newEdgeColor;\n\
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
	texCoord = inTexCoord0;\n\
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
uniform sampler1D texFogDensityTable;\n\
uniform bool stateEnableFogAlphaOnly;\n\
uniform vec4 stateFogColor;\n\
\n\
void main()\n\
{\n\
	vec4 inFragColor = texture2D(texInFragColor, texCoord);\n\
	float inFragDepth = texture2D(texInFragDepth, texCoord).r;\n\
	vec4 inFogAttributes = texture2D(texInFogAttributes, texCoord);\n\
	bool polyEnableFog = (inFogAttributes.r > 0.999);\n\
	vec4 newFoggedColor = inFragColor;\n\
	\n\
	float fogMixWeight = 0.0;\n\
	if (FOG_STEP == 0)\n\
	{\n\
		fogMixWeight = texture1D( texFogDensityTable, (inFragDepth <= FOG_OFFSETF) ? 0.0 : 1.0 ).r;\n\
	}\n\
	else\n\
	{\n\
		fogMixWeight = texture1D( texFogDensityTable, (inFragDepth * (1024.0/float(FOG_STEP))) + (((-float(FOG_OFFSET)/float(FOG_STEP)) - 0.5) / 32.0) ).r;\n\
	}\n\
	\n\
	if (polyEnableFog)\n\
	{\n\
		newFoggedColor = mix(inFragColor, (stateEnableFogAlphaOnly) ? vec4(inFragColor.rgb, stateFogColor.a) : stateFogColor, fogMixWeight);\n\
	}\n\
	\n\
	gl_FragColor = newFoggedColor;\n\
}\n\
"};

// Vertex shader for the final framebuffer, GLSL 1.00
static const char *FramebufferOutputVtxShader_100 = {"\
attribute vec2 inPosition;\n\
attribute vec2 inTexCoord0;\n\
varying vec2 texCoord;\n\
\n\
void main()\n\
{\n\
	texCoord = vec2(inTexCoord0.x, 1.0 - inTexCoord0.y);\n\
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
	gl_FragColor = (colorRGBA6665 / 255.0);\n\
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
	gl_FragColor = texture2D(texInFragColor, texCoord).bgra;\n\
}\n\
"};

bool IsOpenGLDriverVersionSupported(unsigned int checkVersionMajor, unsigned int checkVersionMinor, unsigned int checkVersionRevision)
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
	const char *versionStrDot = strstr(oglVersionString, ".");
	if (versionStrDot == NULL)
	{
		return;
	}
	
	// Next, check for a space that is after the dot, but before the vendor-specific info (if present).
	versionStringLength = strlen(oglVersionString); // Set our default string length here
	const size_t endCheckLength = versionStringLength - (versionStrDot - oglVersionString); // Maximum possible length to check
	const char *checkStringLimit = (endCheckLength < 10) ? versionStrDot + endCheckLength : versionStrDot + 10; // But we're going to limit ourselves to checking only the first 10 characters
	
	char *versionStrEnd = (char *)versionStrDot;
	while (versionStrEnd < checkStringLimit)
	{
		versionStrEnd++;
		if (*versionStrEnd == ' ')
		{
			versionStringLength = versionStrEnd - oglVersionString;
			break;
		}
	}
	
	// Check for any spaces before the dot. There shouldn't be any text before the version number, so
	// this step shouldn't be necessary. However, some drivers can defy the OpenGL spec and include
	// text before the version number, and so we need to handle such non-compliant drivers just in case.
	char *versionStrStart = (char *)versionStrDot;
	while (versionStrStart > oglVersionString)
	{
		versionStrStart--;
		if (*versionStrStart == ' ')
		{
			versionStrStart++; // Don't include the space we just checked.
			versionStringLength -= versionStrStart - oglVersionString;
			break;
		}
	}
	
	// Copy the version substring and parse it.
	char *versionSubstring = (char *)malloc(versionStringLength * sizeof(char));
	strncpy(versionSubstring, versionStrStart, versionStringLength);
	
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
	_isTexInited = false;
	
	_upscaleBuffer = NULL;
	
	glGenTextures(1, &_texID);
}

OpenGLTexture::~OpenGLTexture()
{
	glDeleteTextures(1, &this->_texID);
}

void OpenGLTexture::Load(bool forceTextureInit)
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
			
			if (forceTextureInit || !this->_isTexInited)
			{
				this->_isTexInited = true;
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_sizeS, this->_sizeT, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureSrc);
			}
			else
			{
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, this->_sizeS, this->_sizeT, GL_RGBA, GL_UNSIGNED_BYTE, textureSrc);
			}
			break;
		}
			
		case 2:
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
			
			this->_Upscale<2>(textureSrc, this->_upscaleBuffer);
			
			if (forceTextureInit || !this->_isTexInited)
			{
				this->_isTexInited = true;
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_sizeS*2, this->_sizeT*2, 0, GL_RGBA, GL_UNSIGNED_BYTE, this->_upscaleBuffer);
				glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, this->_sizeS*1, this->_sizeT*1, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureSrc);
			}
			else
			{
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, this->_sizeS*2, this->_sizeT*2, GL_RGBA, GL_UNSIGNED_BYTE, this->_upscaleBuffer);
				glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, this->_sizeS*1, this->_sizeT*1, GL_RGBA, GL_UNSIGNED_BYTE, textureSrc);
			}
			break;
		}
			
		case 4:
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2);
			
			this->_Upscale<4>(textureSrc, this->_upscaleBuffer);
			
			if (forceTextureInit || !this->_isTexInited)
			{
				this->_isTexInited = true;
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_sizeS*4, this->_sizeT*4, 0, GL_RGBA, GL_UNSIGNED_BYTE, this->_upscaleBuffer);
				
				this->_Upscale<2>(textureSrc, this->_upscaleBuffer);
				glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, this->_sizeS*2, this->_sizeT*2, 0, GL_RGBA, GL_UNSIGNED_BYTE, this->_upscaleBuffer);
				
				glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, this->_sizeS*1, this->_sizeT*1, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureSrc);
			}
			else
			{
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, this->_sizeS*4, this->_sizeT*4, GL_RGBA, GL_UNSIGNED_BYTE, this->_upscaleBuffer);
				
				this->_Upscale<2>(textureSrc, this->_upscaleBuffer);
				glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, this->_sizeS*2, this->_sizeT*2, GL_RGBA, GL_UNSIGNED_BYTE, this->_upscaleBuffer);
				
				glTexSubImage2D(GL_TEXTURE_2D, 2, 0, 0, this->_sizeS*1, this->_sizeT*1, GL_RGBA, GL_UNSIGNED_BYTE, textureSrc);
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

template <OpenGLVariantID VARIANTID>
static Render3D* OpenGLRendererCreate()
{
	OpenGLRenderer *newRenderer = NULL;
	Render3DError error = OGLERROR_NOERR;
	
	if (oglrender_init == NULL)
	{
		INFO("OpenGL: oglrender_init is unassigned. Clients must assign this function pointer and have a working context before running OpenGL.\n");
		return newRenderer;
	}
	
	if (oglrender_beginOpenGL == NULL)
	{
		INFO("OpenGL: oglrender_beginOpenGL is unassigned. Clients must assign this function pointer before running OpenGL.\n");
		return newRenderer;
	}
	
	if (oglrender_endOpenGL == NULL)
	{
		INFO("OpenGL: oglrender_endOpenGL is unassigned. Clients must assign this function pointer before running OpenGL.\n");
		return newRenderer;
	}
	
	if (!oglrender_init())
	{
		return newRenderer;
	}
	
	if (!BEGINGL())
	{
		char variantString[32] = {0};
		
		switch (VARIANTID)
		{
			case OpenGLVariantID_Legacy_1_2:
				strncpy(variantString, "OpenGL 1.2 (forced)", sizeof(variantString));
				break;
				
			case OpenGLVariantID_Legacy_2_0:
				strncpy(variantString, "OpenGL 2.0 (forced)", sizeof(variantString));
				break;
				
			case OpenGLVariantID_Legacy_2_1:
				strncpy(variantString, "OpenGL 2.1 (forced)", sizeof(variantString));
				break;
				
			case OpenGLVariantID_CoreProfile_3_2:
				strncpy(variantString, "OpenGL 3.2 (forced)", sizeof(variantString));
				break;
				
			case OpenGLVariantID_LegacyAuto:
			case OpenGLVariantID_StandardAuto:
				strncpy(variantString, "OpenGL (auto)", sizeof(variantString));
				break;
				
			case OpenGLVariantID_ES_3_0:
				strncpy(variantString, "OpenGL ES 3.0", sizeof(variantString));
				break;
				
			default:
				break;
		}
		
		INFO("%s: Could not initialize -- BEGINGL() failed.\n", variantString);
		return newRenderer;
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
	
	if (!IsOpenGLDriverVersionSupported(OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MAJOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MINOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_REVISION))
	{
		INFO("OpenGL: Driver does not support OpenGL v%u.%u.%u or later. Disabling 3D renderer.\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
		     OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MAJOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MINOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_REVISION,
		     oglVersionString, oglVendorString, oglRendererString);
		
		ENDGL();
		return newRenderer;
	}
	
	// Create new OpenGL rendering object
	if (VARIANTID == OpenGLVariantID_ES_3_0)
	{
		if ( (OGLLoadEntryPoints_ES_3_0_Func != NULL) && (OGLCreateRenderer_ES_3_0_Func != NULL) )
		{
			OGLLoadEntryPoints_ES_3_0_Func();
			OGLLoadEntryPoints_Legacy();
			OGLCreateRenderer_ES_3_0_Func(&newRenderer);
		}
		else
		{
			ENDGL();
			return newRenderer;
		}
	}
	else
	{
		if ( (VARIANTID == OpenGLVariantID_StandardAuto) || (VARIANTID == OpenGLVariantID_CoreProfile_3_2) )
		{
			if ( (OGLLoadEntryPoints_3_2_Func != NULL) && (OGLCreateRenderer_3_2_Func != NULL) )
			{
				OGLLoadEntryPoints_3_2_Func();
				OGLLoadEntryPoints_Legacy(); //zero 04-feb-2013 - this seems to be necessary as well
				OGLCreateRenderer_3_2_Func(&newRenderer);
			}
			else if (VARIANTID == OpenGLVariantID_CoreProfile_3_2)
			{
				ENDGL();
				return newRenderer;
			}
		}
		
		if ( (VARIANTID == OpenGLVariantID_StandardAuto) || (VARIANTID == OpenGLVariantID_LegacyAuto) )
		{
			// If the renderer doesn't initialize with OpenGL v3.2 or higher, fall back
			// to one of the lower versions.
			if (newRenderer == NULL)
			{
				OGLLoadEntryPoints_Legacy();
				if (IsOpenGLDriverVersionSupported(2, 1, 0))
				{
					newRenderer = new OpenGLRenderer_2_1;
					newRenderer->SetVersion(2, 1, 0);
				}
				else if (IsOpenGLDriverVersionSupported(2, 0, 0))
				{
					newRenderer = new OpenGLRenderer_2_0;
					newRenderer->SetVersion(2, 0, 0);
				}
				else if (IsOpenGLDriverVersionSupported(1, 2, 0))
				{
					newRenderer = new OpenGLRenderer_1_2;
					newRenderer->SetVersion(1, 2, 0);
				}
			}
		}
		else if (VARIANTID == OpenGLVariantID_Legacy_2_1)
		{
			OGLLoadEntryPoints_Legacy();
			if (IsOpenGLDriverVersionSupported(2, 1, 0))
			{
				newRenderer = new OpenGLRenderer_2_1;
				newRenderer->SetVersion(2, 1, 0);
			}
		}
		else if (VARIANTID == OpenGLVariantID_Legacy_2_0)
		{
			OGLLoadEntryPoints_Legacy();
			if (IsOpenGLDriverVersionSupported(2, 0, 0))
			{
				newRenderer = new OpenGLRenderer_2_0;
				newRenderer->SetVersion(2, 0, 0);
			}
		}
		else if (VARIANTID == OpenGLVariantID_Legacy_1_2)
		{
			OGLLoadEntryPoints_Legacy();
			if (IsOpenGLDriverVersionSupported(1, 2, 0))
			{
				newRenderer = new OpenGLRenderer_1_2;
				newRenderer->SetVersion(1, 2, 0);
			}
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
			INFO("OpenGL: This driver does not support the minimum feature set required to run this renderer. Disabling 3D renderer.\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
			     oglVersionString, oglVendorString, oglRendererString);
		}
		else if (newRenderer->IsVersionSupported(1, 5, 0) && (error == OGLERROR_VBO_UNSUPPORTED))
		{
			INFO("OpenGL: VBOs are not available, even though this version of OpenGL requires them. Disabling 3D renderer.\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
			     oglVersionString, oglVendorString, oglRendererString);
		}
		else if ( newRenderer->IsVersionSupported(2, 0, 0) &&
		         (error == OGLERROR_SHADER_CREATE_ERROR ||
		          error == OGLERROR_VERTEX_SHADER_PROGRAM_LOAD_ERROR ||
		          error == OGLERROR_FRAGMENT_SHADER_PROGRAM_LOAD_ERROR) )
		{
			INFO("OpenGL: Shaders are not working, even though they should be on this version of OpenGL. Disabling 3D renderer.\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
			     oglVersionString, oglVendorString, oglRendererString);
		}
		else if (newRenderer->IsVersionSupported(2, 1, 0) && (error == OGLERROR_PBO_UNSUPPORTED))
		{
			INFO("OpenGL: PBOs are not available, even though this version of OpenGL requires them. Disabling 3D renderer.\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
			     oglVersionString, oglVendorString, oglRendererString);
		}
		else if (newRenderer->IsVersionSupported(3, 0, 0) && (error == OGLERROR_FBO_CREATE_ERROR) && (OGLLoadEntryPoints_3_2_Func != NULL))
		{
			INFO("OpenGL: FBOs are not available, even though this version of OpenGL requires them. Disabling 3D renderer.\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
			     oglVersionString, oglVendorString, oglRendererString);
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
	OpenGLRendererCreate<OpenGLVariantID_StandardAuto>,
	OpenGLRendererDestroy
};

//forcibly use old profile
GPU3DInterface gpu3DglOld = {
	"OpenGL Old",
	OpenGLRendererCreate<OpenGLVariantID_LegacyAuto>,
	OpenGLRendererDestroy
};

//forcibly use new profile
GPU3DInterface gpu3Dgl_3_2 = {
	"OpenGL 3.2",
	OpenGLRendererCreate<OpenGLVariantID_CoreProfile_3_2>,
	OpenGLRendererDestroy
};

// OpenGL ES 3.0 (this is the only version of ES that is supported right now)
GPU3DInterface gpu3Dgl_ES_3_0 = {
	"OpenGL ES 3.0",
	OpenGLRendererCreate<OpenGLVariantID_ES_3_0>,
	OpenGLRendererDestroy
};

OpenGLRenderer::OpenGLRenderer()
{
	_variantID = OpenGLVariantID_LegacyAuto;
	
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
	_isFBOBlitSupported = false;
	isMultisampledFBOSupported = false;
	isShaderSupported = false;
	isVAOSupported = false;
	_isDepthLEqualPolygonFacingSupported = false;
	willFlipOnlyFramebufferOnGPU = false;
	willFlipAndConvertFramebufferOnGPU = false;
	willUsePerSampleZeroDstPass = false;
	
	_emulateShadowPolygon = true;
	_emulateSpecialZeroAlphaBlending = true;
	_emulateNDSDepthCalculation = true;
	_emulateDepthLEqualPolygonFacing = false;
	
	// Init OpenGL rendering states
	ref = (OGLRenderRef *)malloc(sizeof(OGLRenderRef));
	memset(ref, 0, sizeof(OGLRenderRef));
	
	_mappedFramebuffer = NULL;
	_workingTextureUnpackBuffer = (Color4u8 *)malloc_alignedCacheLine(1024 * 1024 * sizeof(Color4u8));
	_pixelReadNeedsFinish = false;
	_needsZeroDstAlphaPass = true;
	_currentPolyIndex = 0;
	_enableAlphaBlending = true;
	_lastTextureDrawTarget = OGLTextureUnitID_GColor;
	_geometryProgramFlags.value = 0;
	_fogProgramKey.key = 0;
	_fogProgramMap.clear();
	_clearImageIndex = 0;
	
	memset(&_pendingRenderStates, 0, sizeof(_pendingRenderStates));
}

OpenGLRenderer::~OpenGLRenderer()
{
	free_aligned(this->_framebufferColor);
	free_aligned(this->_workingTextureUnpackBuffer);
	
	// Destroy OpenGL rendering states
	free(this->ref);
	this->ref = NULL;
}

bool OpenGLRenderer::IsExtensionPresent(const std::set<std::string> *oglExtensionSet, const std::string extensionName) const
{
	if (oglExtensionSet == NULL || oglExtensionSet->size() == 0)
	{
		return false;
	}
	
	return (oglExtensionSet->find(extensionName) != oglExtensionSet->end());
}

Render3DError OpenGLRenderer::ShaderProgramCreate(GLuint &vtxShaderID,
												  GLuint &fragShaderID,
												  GLuint &programID,
												  const char *vtxShaderCString,
												  const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	
	if (vtxShaderID == 0)
	{
		vtxShaderID = glCreateShader(GL_VERTEX_SHADER);
		if (vtxShaderID == 0)
		{
			INFO("OpenGL: Failed to create the vertex shader.\n");
			return OGLERROR_SHADER_CREATE_ERROR;
		}
		
		const char *vtxShaderCStringPtr = vtxShaderCString;
		glShaderSource(vtxShaderID, 1, (const GLchar **)&vtxShaderCStringPtr, NULL);
		glCompileShader(vtxShaderID);
		if (!this->ValidateShaderCompile(GL_VERTEX_SHADER, vtxShaderID))
		{
			error = OGLERROR_SHADER_CREATE_ERROR;
			return error;
		}
	}
	
	if (fragShaderID == 0)
	{
		fragShaderID = glCreateShader(GL_FRAGMENT_SHADER);
		if (fragShaderID == 0)
		{
			INFO("OpenGL: Failed to create the fragment shader.\n");
			error = OGLERROR_SHADER_CREATE_ERROR;
			return error;
		}
		
		const char *fragShaderCStringPtr = fragShaderCString;
		glShaderSource(fragShaderID, 1, (const GLchar **)&fragShaderCStringPtr, NULL);
		glCompileShader(fragShaderID);
		if (!this->ValidateShaderCompile(GL_FRAGMENT_SHADER, fragShaderID))
		{
			error = OGLERROR_SHADER_CREATE_ERROR;
			return error;
		}
	}
	
	programID = glCreateProgram();
	if (programID == 0)
	{
		INFO("OpenGL: Failed to create the shader program.\n");
		error = OGLERROR_SHADER_CREATE_ERROR;
		return error;
	}
	
	glAttachShader(programID, vtxShaderID);
	glAttachShader(programID, fragShaderID);
	
	return error;
}

bool OpenGLRenderer::ValidateShaderCompile(GLenum shaderType, GLuint theShader) const
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
		
		switch (shaderType)
		{
			case GL_VERTEX_SHADER:
				INFO("OpenGL: FAILED TO COMPILE VERTEX SHADER:\n%s\n", log);
				break;
				
			case GL_FRAGMENT_SHADER:
				INFO("OpenGL: FAILED TO COMPILE FRAGMENT SHADER:\n%s\n", log);
				break;
				
			default:
				INFO("OpenGL: FAILED TO COMPILE SHADER:\n%s\n", log);
				break;
		}
		
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
		
		INFO("OpenGL: FAILED TO LINK SHADER PROGRAM:\n%s\n", log);
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

bool OpenGLRenderer::IsVersionSupported(unsigned int checkVersionMajor, unsigned int checkVersionMinor, unsigned int checkVersionRevision) const
{
	bool result = false;
	
	if ( (this->versionMajor > checkVersionMajor) ||
		 (this->versionMajor >= checkVersionMajor && this->versionMinor > checkVersionMinor) ||
		 (this->versionMajor >= checkVersionMajor && this->versionMinor >= checkVersionMinor && this->versionRevision >= checkVersionRevision) )
	{
		result = true;
	}
	
	return result;
}

Render3DError OpenGLRenderer::_FlushFramebufferFlipAndConvertOnCPU(const Color4u8 *__restrict srcFramebuffer,
																   Color4u8 *__restrict dstFramebufferMain, u16 *__restrict dstFramebuffer16,
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
		if (!doFramebufferConvert)
		{
			if ( (dstFramebufferMain != NULL) && (dstFramebuffer16 != NULL) )
			{
#ifdef ENABLE_SSE2
				const size_t ssePixCount = this->_framebufferPixCount - (this->_framebufferPixCount % 8);
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
				for (; i < this->_framebufferPixCount; i++)
				{
					dstFramebufferMain[i].value = ColorspaceCopy32<false>(srcFramebuffer[i]);
					dstFramebuffer16[i]         = ColorspaceConvert8888To5551<false>(srcFramebuffer[i]);
				}
				
				this->_renderNeedsFlushMain = false;
				this->_renderNeedsFlush16 = false;
			}
			else if (dstFramebufferMain != NULL)
			{
				ColorspaceCopyBuffer32<false, false>((u32 *)srcFramebuffer, (u32 *)dstFramebufferMain, this->_framebufferPixCount);
				this->_renderNeedsFlushMain = false;
			}
			else
			{
				ColorspaceConvertBuffer8888To5551<false, false>((u32 *)srcFramebuffer, dstFramebuffer16, this->_framebufferPixCount);
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
					const size_t ssePixCount = this->_framebufferPixCount - (this->_framebufferPixCount % 8);
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
					for (; i < this->_framebufferPixCount; i++)
					{
						dstFramebufferMain[i].value = ColorspaceConvert8888To6665<true>(srcFramebuffer[i]);
						dstFramebuffer16[i]         = ColorspaceConvert8888To5551<true>(srcFramebuffer[i]);
					}
					
					this->_renderNeedsFlushMain = false;
					this->_renderNeedsFlush16 = false;
				}
				else if (dstFramebufferMain != NULL)
				{
					ColorspaceConvertBuffer8888To6665<true, false>((u32 *)srcFramebuffer, (u32 *)dstFramebufferMain, this->_framebufferPixCount);
					this->_renderNeedsFlushMain = false;
				}
				else
				{
					ColorspaceConvertBuffer8888To5551<true, false>((u32 *)srcFramebuffer, dstFramebuffer16, this->_framebufferPixCount);
					this->_renderNeedsFlush16 = false;
				}
			}
			else if (this->_outputFormat == NDSColorFormat_BGR888_Rev)
			{
				if ( (dstFramebufferMain != NULL) && (dstFramebuffer16 != NULL) )
				{
#ifdef ENABLE_SSE2
					const size_t ssePixCount = this->_framebufferPixCount - (this->_framebufferPixCount % 8);
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
					for (; i < this->_framebufferPixCount; i++)
					{
						dstFramebufferMain[i].value = ColorspaceCopy32<true>(srcFramebuffer[i]);
						dstFramebuffer16[i]         = ColorspaceConvert8888To5551<true>(srcFramebuffer[i]);
					}
					
					this->_renderNeedsFlushMain = false;
					this->_renderNeedsFlush16 = false;
				}
				else if (dstFramebufferMain != NULL)
				{
					ColorspaceCopyBuffer32<true, false>((u32 *)srcFramebuffer, (u32 *)dstFramebufferMain, this->_framebufferPixCount);
					this->_renderNeedsFlushMain = false;
				}
				else
				{
					ColorspaceConvertBuffer8888To5551<true, false>((u32 *)srcFramebuffer, dstFramebuffer16, this->_framebufferPixCount);
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
						dstFramebufferMain[iw].value = ColorspaceCopy32<false>(srcFramebuffer[ir]);
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
							dstFramebufferMain[iw].value = ColorspaceConvert8888To6665<true>(srcFramebuffer[ir]);
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
							dstFramebufferMain[iw].value = ColorspaceCopy32<true>(srcFramebuffer[ir]);
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

Render3DError OpenGLRenderer::FlushFramebuffer(const Color4u8 *__restrict srcFramebuffer, Color4u8 *__restrict dstFramebufferMain, u16 *__restrict dstFramebuffer16)
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

Color4u8* OpenGLRenderer::GetFramebuffer()
{
	return (this->willFlipAndConvertFramebufferOnGPU && this->isPBOSupported) ? this->_mappedFramebuffer : GPU->GetEngineMain()->Get3DFramebufferMain();
}

GLsizei OpenGLRenderer::GetLimitedMultisampleSize() const
{
	u32 deviceMultisamples = this->_deviceInfo.maxSamples;
	u32 workingMultisamples = (u32)this->_selectedMultisampleSize;
	
	if (workingMultisamples == 1)
	{
		// If this->_selectedMultisampleSize is 1, then just set workingMultisamples to 2
		// by default. This is done to prevent the multisampled FBOs from being resized to
		// a meaningless sample size of 1.
		//
		// As an side, if someone wants to bring back automatic MSAA sample size selection
		// in the future, then this would be the place to reimplement it.
		workingMultisamples = 2;
	}
	else
	{
		// Ensure that workingMultisamples is a power-of-two, which is what OpenGL likes.
		//
		// If workingMultisamples is not a power-of-two, then workingMultisamples is
		// increased to the next largest power-of-two.
		workingMultisamples--;
		workingMultisamples |= workingMultisamples >> 1;
		workingMultisamples |= workingMultisamples >> 2;
		workingMultisamples |= workingMultisamples >> 4;
		workingMultisamples |= workingMultisamples >> 8;
		workingMultisamples |= workingMultisamples >> 16;
		workingMultisamples++;
	}
	
	if (deviceMultisamples > workingMultisamples)
	{
		deviceMultisamples = workingMultisamples;
	}
	
	return (GLsizei)deviceMultisamples;
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
size_t OpenGLRenderer::DrawPolygonsForIndexRange(const POLY *rawPolyList, const CPoly *clippedPolyList, const size_t clippedPolyCount, size_t firstIndex, size_t lastIndex, size_t &indexOffset, POLYGON_ATTR &lastPolyAttr)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (lastIndex > (clippedPolyCount - 1))
	{
		lastIndex = clippedPolyCount - 1;
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
	static const GLenum oglPrimitiveType[]	= {
		GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES, GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_STRIP, GL_LINE_STRIP, // Normal polygons
		GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_LOOP    // Wireframe polygons
	};
	
	static const GLsizei indexIncrementLUT[] = {
		3, 6, 3, 6, 3, 4, 3, 4, // Normal polygons
		3, 4, 3, 4, 3, 4, 3, 4  // Wireframe polygons
	};
	
	// Set up the initial polygon
	const CPoly &initialClippedPoly = clippedPolyList[firstIndex];
	const POLY &initialRawPoly = rawPolyList[initialClippedPoly.index];
	TEXIMAGE_PARAM lastTexParams = initialRawPoly.texParam;
	u32 lastTexPalette = initialRawPoly.texPalette;
	GFX3D_Viewport lastViewport = initialRawPoly.viewport;
	
	this->SetupTexture(initialRawPoly, firstIndex);
	this->SetupViewport(initialRawPoly.viewport);
	
	// Enumerate through all polygons and render
	GLsizei vertIndexCount = 0;
	GLushort *indexBufferPtr = (this->isVBOSupported) ? (GLushort *)NULL + indexOffset : OGLRef.vertIndexBuffer + indexOffset;
	
	for (size_t i = firstIndex; i <= lastIndex; i++)
	{
		const CPoly &clippedPoly = clippedPolyList[i];
		const POLY &rawPoly = rawPolyList[clippedPoly.index];
		
		// Set up the polygon if it changed
		if (lastPolyAttr.value != rawPoly.attribute.value)
		{
			lastPolyAttr = rawPoly.attribute;
			this->SetupPolygon(rawPoly, (DRAWMODE != OGLPolyDrawMode_DrawOpaquePolys), (DRAWMODE != OGLPolyDrawMode_ZeroAlphaPass), clippedPoly.isPolyBackFacing);
		}
		
		// Set up the texture if it changed
		if (lastTexParams.value != rawPoly.texParam.value || lastTexPalette != rawPoly.texPalette)
		{
			lastTexParams = rawPoly.texParam;
			lastTexPalette = rawPoly.texPalette;
			this->SetupTexture(rawPoly, i);
		}
		
		// Set up the viewport if it changed
		if (lastViewport.value != rawPoly.viewport.value)
		{
			lastViewport = rawPoly.viewport;
			this->SetupViewport(rawPoly.viewport);
		}
		
		// In wireframe mode, redefine all primitives as GL_LINE_LOOP rather than
		// setting the polygon mode to GL_LINE though glPolygonMode(). Not only is
		// drawing more accurate this way, but it also allows GFX3D_QUADS and
		// GFX3D_QUAD_STRIP primitives to properly draw as wireframe without the
		// extra diagonal line.
		const size_t LUTIndex = (!GFX3D_IsPolyWireframe(rawPoly)) ? rawPoly.vtxFormat : (0x08 | rawPoly.vtxFormat);
		const GLenum polyPrimitive = oglPrimitiveType[LUTIndex];
		
		// Increment the vertex count
		vertIndexCount += indexIncrementLUT[LUTIndex];
		
		// Look ahead to the next polygon to see if we can simply buffer the indices
		// instead of uploading them now. We can buffer if all polygon states remain
		// the same and we're not drawing a line loop or line strip.
		if (i+1 <= lastIndex)
		{
			const CPoly &nextClippedPoly = clippedPolyList[i+1];
			const POLY &nextRawPoly = rawPolyList[nextClippedPoly.index];
			
			if (lastPolyAttr.value == nextRawPoly.attribute.value &&
				lastTexParams.value == nextRawPoly.texParam.value &&
				lastTexPalette == nextRawPoly.texPalette &&
				lastViewport.value == nextRawPoly.viewport.value &&
				polyPrimitive == oglPrimitiveType[nextRawPoly.vtxFormat] &&
				polyPrimitive != GL_LINE_LOOP &&
				polyPrimitive != GL_LINE_STRIP &&
				oglPrimitiveType[nextRawPoly.vtxFormat] != GL_LINE_LOOP &&
				oglPrimitiveType[nextRawPoly.vtxFormat] != GL_LINE_STRIP &&
				clippedPoly.isPolyBackFacing == nextClippedPoly.isPolyBackFacing)
			{
				continue;
			}
		}
		
		// Render the polygons
		this->SetPolygonIndex(i);
		
		if (rawPoly.attribute.Mode == POLYGON_MODE_SHADOW)
		{
			if ((DRAWMODE != OGLPolyDrawMode_ZeroAlphaPass) && this->_emulateShadowPolygon)
			{
				this->DrawShadowPolygon(polyPrimitive,
				                        vertIndexCount,
				                        indexBufferPtr,
				                        rawPoly.attribute.DepthEqualTest_Enable,
				                        rawPoly.attribute.TranslucentDepthWrite_Enable,
				                        (DRAWMODE == OGLPolyDrawMode_DrawTranslucentPolys), rawPoly.attribute.PolygonID);
			}
		}
		else if ( (rawPoly.texParam.PackedFormat == TEXMODE_A3I5) || (rawPoly.texParam.PackedFormat == TEXMODE_A5I3) )
		{
			this->DrawAlphaTexturePolygon<DRAWMODE>(polyPrimitive,
			                                        vertIndexCount,
			                                        indexBufferPtr,
			                                        rawPoly.attribute.DepthEqualTest_Enable,
			                                        rawPoly.attribute.TranslucentDepthWrite_Enable,
			                                        GFX3D_IsPolyWireframe(rawPoly) || GFX3D_IsPolyOpaque(rawPoly),
			                                        rawPoly.attribute.PolygonID,
			                                        !clippedPoly.isPolyBackFacing);
		}
		else
		{
			this->DrawOtherPolygon<DRAWMODE>(polyPrimitive,
			                                 vertIndexCount,
			                                 indexBufferPtr,
			                                 rawPoly.attribute.DepthEqualTest_Enable,
			                                 rawPoly.attribute.TranslucentDepthWrite_Enable,
			                                 rawPoly.attribute.PolygonID,
			                                 !clippedPoly.isPolyBackFacing);
		}
		
		indexBufferPtr += vertIndexCount;
		indexOffset += vertIndexCount;
		vertIndexCount = 0;
	}
	
	return indexOffset;
}

template <OGLPolyDrawMode DRAWMODE>
Render3DError OpenGLRenderer::DrawAlphaTexturePolygon(const GLenum polyPrimitive,
													  const GLsizei vertIndexCount,
													  const GLushort *indexBufferPtr,
													  const bool performDepthEqualTest,
													  const bool enableAlphaDepthWrite,
													  const bool canHaveOpaqueFragments,
													  const u8 opaquePolyID,
													  const bool isPolyFrontFacing)
{
	const OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isShaderSupported)
	{
		if ((DRAWMODE != OGLPolyDrawMode_ZeroAlphaPass) && performDepthEqualTest && this->_emulateNDSDepthCalculation)
		{
			if (DRAWMODE == OGLPolyDrawMode_DrawTranslucentPolys)
			{
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
				glDepthMask(GL_FALSE);
				
				// Use the stencil buffer to determine which fragments pass the lower-side tolerance.
				glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], (float)DEPTH_EQUALS_TEST_TOLERANCE / 16777215.0f);
				glDepthFunc(GL_LEQUAL);
				glStencilFunc(GL_ALWAYS, 0x80, 0x80);
				glStencilOp(GL_ZERO, GL_ZERO, GL_REPLACE);
				glStencilMask(0x80);
				
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				if (canHaveOpaqueFragments)
				{
					glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_TRUE);
					glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
					glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_FALSE);
				}
				
				// Use the stencil buffer to determine which fragments pass the higher-side tolerance.
				glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], (float)-DEPTH_EQUALS_TEST_TOLERANCE / 16777215.0f);
				glDepthFunc(GL_GEQUAL);
				glStencilFunc(GL_EQUAL, 0x80, 0x80);
				glStencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
				glStencilMask(0x80);
				
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				if (canHaveOpaqueFragments)
				{
					glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_TRUE);
					glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
					glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_FALSE);
				}
				
				// Set up the actual drawing of the polygon, using the mask within the stencil buffer to determine which fragments should pass.
				glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], 0.0f);
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
					glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_TRUE);
					glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
					glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_FALSE);
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
					glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_TRUE);
					glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
					glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_FALSE);
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
				
				glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_TRUE);
				
				// Use the stencil buffer to determine which fragments pass the lower-side tolerance.
				glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], (float)DEPTH_EQUALS_TEST_TOLERANCE / 16777215.0f);
				glDepthFunc(GL_LEQUAL);
				glStencilFunc(GL_ALWAYS, 0x80, 0x80);
				glStencilOp(GL_ZERO, GL_ZERO, GL_REPLACE);
				glStencilMask(0x80);
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				// Use the stencil buffer to determine which fragments pass the higher-side tolerance.
				glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], (float)-DEPTH_EQUALS_TEST_TOLERANCE / 16777215.0f);
				glDepthFunc(GL_GEQUAL);
				glStencilFunc(GL_EQUAL, 0x80, 0x80);
				glStencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
				glStencilMask(0x80);
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				// Set up the actual drawing of the polygon, using the mask within the stencil buffer to determine which fragments should pass.
				glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], 0.0f);
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
				
				glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_FALSE);
			}
		}
		else if (DRAWMODE != OGLPolyDrawMode_DrawOpaquePolys)
		{
			// Draw the translucent fragments.
			if (this->_emulateDepthLEqualPolygonFacing && this->_isDepthLEqualPolygonFacingSupported && isPolyFrontFacing)
			{
				glDepthFunc(GL_EQUAL);
				glUniform1i(OGLRef.uniformDrawModeDepthEqualsTest[this->_geometryProgramFlags.value], GL_TRUE);
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				glDepthFunc(GL_LESS);
				glUniform1i(OGLRef.uniformDrawModeDepthEqualsTest[this->_geometryProgramFlags.value], GL_FALSE);
			}
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
				
				glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_TRUE);
				if (this->_emulateDepthLEqualPolygonFacing && this->_isDepthLEqualPolygonFacingSupported && isPolyFrontFacing)
				{
					glDepthFunc(GL_EQUAL);
					glUniform1i(OGLRef.uniformDrawModeDepthEqualsTest[this->_geometryProgramFlags.value], GL_TRUE);
					glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
					
					glDepthFunc(GL_LESS);
					glUniform1i(OGLRef.uniformDrawModeDepthEqualsTest[this->_geometryProgramFlags.value], GL_FALSE);
				}
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_FALSE);
				
				if (DRAWMODE != OGLPolyDrawMode_ZeroAlphaPass)
				{
					glStencilFunc(GL_NOTEQUAL, 0x40 | opaquePolyID, 0x7F);
					glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
					glDepthMask((enableAlphaDepthWrite) ? GL_TRUE : GL_FALSE);
				}
			}
		}
		else // Draw the polygon as completely opaque.
		{
			glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_TRUE);
			
			if (this->_emulateDepthLEqualPolygonFacing && this->_isDepthLEqualPolygonFacingSupported)
			{
				if (isPolyFrontFacing)
				{
					glDepthFunc(GL_EQUAL);
					glStencilFunc(GL_EQUAL, 0x40 | opaquePolyID, 0x40);
					glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
					
					glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
					glDepthMask(GL_FALSE);
					glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
					glStencilMask(0x40);
					glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
					
					glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
					glDepthMask(GL_TRUE);
					glDepthFunc(GL_LESS);
					glStencilFunc(GL_ALWAYS, opaquePolyID, 0x3F);
					glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
					glStencilMask(0xFF);
					glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				}
				else
				{
					glStencilFunc(GL_ALWAYS, 0x40 | opaquePolyID, 0x40);
					glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
					
					glStencilFunc(GL_ALWAYS, opaquePolyID, 0x3F);
				}
			}
			else
			{
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
			}
			
			glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_FALSE);
		}
	}
	else
	{
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
	}
	
	return OGLERROR_NOERR;
}

template <OGLPolyDrawMode DRAWMODE>
Render3DError OpenGLRenderer::DrawOtherPolygon(const GLenum polyPrimitive,
											   const GLsizei vertIndexCount,
											   const GLushort *indexBufferPtr,
											   const bool performDepthEqualTest,
											   const bool enableAlphaDepthWrite,
											   const u8 opaquePolyID,
											   const bool isPolyFrontFacing)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if ((DRAWMODE != OGLPolyDrawMode_ZeroAlphaPass) && performDepthEqualTest && this->_emulateNDSDepthCalculation && this->isShaderSupported)
	{
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_FALSE);
		
		// Use the stencil buffer to determine which fragments pass the lower-side tolerance.
		glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], (float)DEPTH_EQUALS_TEST_TOLERANCE / 16777215.0f);
		glDepthFunc(GL_LEQUAL);
		glStencilFunc(GL_ALWAYS, 0x80, 0x80);
		glStencilOp(GL_ZERO, GL_ZERO, GL_REPLACE);
		glStencilMask(0x80);
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		
		// Use the stencil buffer to determine which fragments pass the higher-side tolerance.
		glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], (float)-DEPTH_EQUALS_TEST_TOLERANCE / 16777215.0f);
		glDepthFunc(GL_GEQUAL);
		glStencilFunc(GL_EQUAL, 0x80, 0x80);
		glStencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
		glStencilMask(0x80);
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		
		// Set up the actual drawing of the polygon.
		glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], 0.0f);
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
	else if (DRAWMODE == OGLPolyDrawMode_DrawOpaquePolys)
	{
		if (this->_emulateDepthLEqualPolygonFacing && this->_isDepthLEqualPolygonFacingSupported)
		{
			if (isPolyFrontFacing)
			{
				glDepthFunc(GL_EQUAL);
				glStencilFunc(GL_EQUAL, 0x40 | opaquePolyID, 0x40);
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
				glDepthMask(GL_FALSE);
				glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
				glStencilMask(0x40);
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glDepthMask(GL_TRUE);
				glDepthFunc(GL_LESS);
				glStencilFunc(GL_ALWAYS, opaquePolyID, 0x3F);
				glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
				glStencilMask(0xFF);
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
			}
			else
			{
				glStencilFunc(GL_ALWAYS, 0x40 | opaquePolyID, 0x40);
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
				
				glStencilFunc(GL_ALWAYS, opaquePolyID, 0x3F);
			}
		}
		else
		{
			glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		}
	}
	else
	{
		if (this->_emulateDepthLEqualPolygonFacing && this->_isDepthLEqualPolygonFacingSupported && isPolyFrontFacing)
		{
			glDepthFunc(GL_EQUAL);
			glUniform1i(OGLRef.uniformDrawModeDepthEqualsTest[this->_geometryProgramFlags.value], GL_TRUE);
			glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
			
			glDepthFunc(GL_LESS);
			glUniform1i(OGLRef.uniformDrawModeDepthEqualsTest[this->_geometryProgramFlags.value], GL_FALSE);
		}
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer::ApplyRenderingSettings(const GFX3D_State &renderState)
{
	Render3DError error = RENDER3DERROR_NOERR;
	
	const bool didSelectedMultisampleSizeChange = (this->_selectedMultisampleSize != CommonSettings.GFX3D_Renderer_MultisampleSize);
	const bool didEmulateNDSDepthCalculationChange = (this->_emulateNDSDepthCalculation != CommonSettings.OpenGL_Emulation_NDSDepthCalculation);
	const bool didEnableTextureSmoothingChange = (this->_enableTextureSmoothing != CommonSettings.GFX3D_Renderer_TextureSmoothing);
	const bool didEmulateDepthLEqualPolygonFacingChange = (this->_emulateDepthLEqualPolygonFacing != (CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing && this->_isDepthLEqualPolygonFacingSupported));
	
	this->_emulateShadowPolygon = CommonSettings.OpenGL_Emulation_ShadowPolygon;
	this->_emulateSpecialZeroAlphaBlending = CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending;
	this->_emulateNDSDepthCalculation = CommonSettings.OpenGL_Emulation_NDSDepthCalculation;
	this->_emulateDepthLEqualPolygonFacing = CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing && this->_isDepthLEqualPolygonFacingSupported;
	
	this->_selectedMultisampleSize = CommonSettings.GFX3D_Renderer_MultisampleSize;
	this->_enableMultisampledRendering = ((this->_selectedMultisampleSize >= 2) && this->isMultisampledFBOSupported);
	
	error = Render3D::ApplyRenderingSettings(renderState);
	if (error != RENDER3DERROR_NOERR)
	{
		return error;
	}
	
	if (didSelectedMultisampleSizeChange ||
		didEmulateNDSDepthCalculationChange ||
		didEnableTextureSmoothingChange ||
		didEmulateDepthLEqualPolygonFacingChange)
	{
		if (!BEGINGL())
		{
			return OGLERROR_BEGINGL_FAILED;
		}
		
		if (didSelectedMultisampleSizeChange)
		{
			GLsizei sampleSize = this->GetLimitedMultisampleSize();
			this->ResizeMultisampledFBOs(sampleSize);
		}
		
		if ( this->isShaderSupported &&
			(didEmulateNDSDepthCalculationChange ||
			 didEnableTextureSmoothingChange ||
			 didEmulateDepthLEqualPolygonFacingChange) )
		{
			glUseProgram(0);
			this->DestroyGeometryPrograms();
			
			error = this->CreateGeometryPrograms();
			if (error != OGLERROR_NOERR)
			{
				glUseProgram(0);
				this->DestroyGeometryPrograms();
				this->isShaderSupported = false;
				
				ENDGL();
				return error;
			}
		}
		
		ENDGL();
	}
	
	return error;
}

OpenGLRenderer_1_2::~OpenGLRenderer_1_2()
{
	glFinish();
	
	_pixelReadNeedsFinish = false;
	
	free_aligned(ref->position4fBuffer);
	ref->position4fBuffer = NULL;
	
	free_aligned(ref->texCoord2fBuffer);
	ref->texCoord2fBuffer = NULL;
	
	free_aligned(ref->color4fBuffer);
	ref->color4fBuffer = NULL;
	
	if (this->isShaderSupported)
	{
		glUseProgram(0);
		
		this->DestroyGeometryPrograms();
		this->DestroyGeometryZeroDstAlphaProgram();
		this->DestroyEdgeMarkProgram();
		this->DestroyFogPrograms();
		this->DestroyFramebufferOutput6665Programs();
		this->DestroyFramebufferOutput8888Programs();
	}
	
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
	
#if !defined(GL_ES_VERSION_3_0)
	if (!this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_multitexture"))
	{
		return OGLERROR_DRIVER_VERSION_TOO_OLD;
	}
	else
	{
		GLint maxFixedFunctionTexUnitsOGL = 0;
		glGetIntegerv(GL_MAX_TEXTURE_UNITS, &maxFixedFunctionTexUnitsOGL);
		
		if (maxFixedFunctionTexUnitsOGL < 4)
		{
			return OGLERROR_DRIVER_VERSION_TOO_OLD;
		}
	}
#endif
	
	// Get host GPU device properties
	GLfloat maxAnisotropyOGL = 1.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropyOGL);
	this->_deviceInfo.maxAnisotropy = maxAnisotropyOGL;
	
	// This is traditionally the fastest format and data type for glReadPixels in legacy mode.
	OGLRef.readPixelsBestFormat = GL_BGRA;
	OGLRef.readPixelsBestDataType = GL_UNSIGNED_BYTE;
	
	// Need to generate this texture first because FBO creation needs it.
	// This texture is only required by shaders, and so if shader creation
	// fails, then we can immediately delete this texture if an error occurs.
	glGenTextures(1, &OGLRef.texFinalColorID);
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FinalColor);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texFinalColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glActiveTexture(GL_TEXTURE0);
	
	this->isVBOSupported = this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_buffer_object");
	if (this->isVBOSupported)
	{
		this->CreateVBOs();
	}
	else if (this->IsVersionSupported(1, 5, 0))
	{
		error = OGLERROR_VBO_UNSUPPORTED;
		return error;
	}
	
	this->isPBOSupported	= this->isVBOSupported &&
							 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_pixel_buffer_object") ||
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_pixel_buffer_object"));
	if (this->isPBOSupported)
	{
		this->CreatePBOs();
	}
	else if (this->IsVersionSupported(2, 1, 0))
	{
		error = OGLERROR_PBO_UNSUPPORTED;
		return error;
	}
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
	this->isFBOSupported = this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_object") &&
	                       this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_packed_depth_stencil");
	if (this->isFBOSupported)
	{
		GLint maxColorAttachments = 0;
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &maxColorAttachments);
		
		if (maxColorAttachments >= 4)
		{
			error = this->CreateFBOs();
			if (error != OGLERROR_NOERR)
			{
				this->isFBOSupported = false;
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
	
	this->_isFBOBlitSupported = this->isFBOSupported &&
	                            this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_blit");
	if (!this->_isFBOBlitSupported)
	{
		INFO("OpenGL: FBO blitting is unsupported. Some emulation features will be disabled.\n");
	}
	
	this->_selectedMultisampleSize = CommonSettings.GFX3D_Renderer_MultisampleSize;
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
	this->isMultisampledFBOSupported = this->_isFBOBlitSupported &&
	                                   this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_multisample");
	if (this->isMultisampledFBOSupported)
	{
		GLint maxSamplesOGL = 0;
		glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSamplesOGL);
		this->_deviceInfo.maxSamples = (u8)maxSamplesOGL;
		
		if (this->_deviceInfo.maxSamples >= 2)
		{
			// Try and initialize the multisampled FBOs with the GFX3D_Renderer_MultisampleSize.
			// However, if the client has this set to 0, then set sampleSize to 2 in order to
			// force the generation and the attachments of the buffers at a meaningful sample
			// size. If GFX3D_Renderer_MultisampleSize is 0, then we can deallocate the buffer
			// memory afterwards.
			GLsizei sampleSize = this->GetLimitedMultisampleSize();
			if (sampleSize == 0)
			{
				sampleSize = 2;
			}
			
			error = this->CreateMultisampledFBO(sampleSize);
			if (error != OGLERROR_NOERR)
			{
				this->isMultisampledFBOSupported = false;
			}
			
			// If GFX3D_Renderer_MultisampleSize is 0, then we can deallocate the buffers now
			// in order to save some memory.
			if (this->_selectedMultisampleSize == 0)
			{
				this->ResizeMultisampledFBOs(0);
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
	
	this->isShaderSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_shader_objects") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_shader") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_fragment_shader") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_program");
	if (this->isShaderSupported)
	{
		GLint maxColorAttachmentsOGL = 0;
		GLint maxDrawBuffersOGL = 0;
		GLint maxShaderTexUnitsOGL = 0;
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &maxColorAttachmentsOGL);
		glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffersOGL);
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxShaderTexUnitsOGL);
		
		if ( (maxColorAttachmentsOGL >= 4) && (maxDrawBuffersOGL >= 4) && (maxShaderTexUnitsOGL >= 8) )
		{
			this->_enableTextureSmoothing = CommonSettings.GFX3D_Renderer_TextureSmoothing;
			this->_emulateShadowPolygon = CommonSettings.OpenGL_Emulation_ShadowPolygon;
			this->_emulateSpecialZeroAlphaBlending = CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending;
			this->_emulateNDSDepthCalculation = CommonSettings.OpenGL_Emulation_NDSDepthCalculation;
			this->_emulateDepthLEqualPolygonFacing = CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing;
			
			error = this->CreateGeometryPrograms();
			if (error == OGLERROR_NOERR)
			{
				error = this->CreateGeometryZeroDstAlphaProgram(GeometryZeroDstAlphaPixelMaskVtxShader_100, GeometryZeroDstAlphaPixelMaskFragShader_100);
				if (error == OGLERROR_NOERR)
				{
					INFO("OpenGL: Successfully created geometry shaders.\n");
					
					error = this->InitPostprocessingPrograms(EdgeMarkVtxShader_100,
															 EdgeMarkFragShader_100,
															 FramebufferOutputVtxShader_100,
															 FramebufferOutputRGBA6665FragShader_100,
															 FramebufferOutputRGBA8888FragShader_100);
				}
			}
			
			if (error != OGLERROR_NOERR)
			{
				glUseProgram(0);
				this->DestroyGeometryPrograms();
				this->DestroyGeometryZeroDstAlphaProgram();
				this->isShaderSupported = false;
			}
		}
		else
		{
			INFO("OpenGL: Driver does not support at least 4 color attachments, 4 draw buffers, and 8 texture image units.\n");
			this->isShaderSupported = false;
		}
	}
	
	if (!this->isShaderSupported)
	{
		INFO("OpenGL: Shaders are unsupported. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		
		glDeleteTextures(1, &OGLRef.texFinalColorID);
		OGLRef.texFinalColorID = 0;
		
		if (this->IsVersionSupported(2, 0, 0))
		{
			return error;
		}
		
		// If shaders aren't available, we need to take care of the vertex data format
		// conversion on the CPU instead of on the GPU using these client-side buffers.
		OGLRef.position4fBuffer = (GLfloat *)malloc_alignedPage(VERTLIST_SIZE * sizeof(GLfloat) * 4);
		OGLRef.texCoord2fBuffer = (GLfloat *)malloc_alignedPage(VERTLIST_SIZE * sizeof(GLfloat) * 2);
		OGLRef.color4fBuffer    = (GLfloat *)malloc_alignedPage(VERTLIST_SIZE * sizeof(GLfloat) * 4);
	}
	
	this->isVAOSupported = this->isVBOSupported &&
	                      (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_array_object") ||
	                       this->IsExtensionPresent(&oglExtensionSet, "GL_APPLE_vertex_array_object"));
	if (this->isVAOSupported)
	{
		this->CreateVAOs();
	}
	
	// Set rendering support flags based on driver features.
	this->willFlipAndConvertFramebufferOnGPU = this->isShaderSupported && this->isVBOSupported;
	this->willFlipOnlyFramebufferOnGPU = this->willFlipAndConvertFramebufferOnGPU || this->_isFBOBlitSupported;
	this->_deviceInfo.isEdgeMarkSupported = this->isShaderSupported && this->isVBOSupported && this->isFBOSupported;
	this->_deviceInfo.isFogSupported = this->isShaderSupported && this->isVBOSupported && this->isFBOSupported;
	this->_deviceInfo.isTextureSmoothingSupported = this->isShaderSupported;
	
	this->_isDepthLEqualPolygonFacingSupported = this->isShaderSupported && this->isVBOSupported && this->isFBOSupported;
	
	this->_enableMultisampledRendering = ((this->_selectedMultisampleSize >= 2) && this->isMultisampledFBOSupported);
	
	this->InitFinalRenderStates(&oglExtensionSet); // This must be done last
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::CreateVBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenBuffers(1, &OGLRef.vboGeometryVtxID);
	glGenBuffers(1, &OGLRef.iboGeometryIndexID);
	glGenBuffers(1, &OGLRef.vboPostprocessVtxID);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
	glBufferData(GL_ARRAY_BUFFER, VERTLIST_SIZE * sizeof(NDSVertex), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(OGLRef.vertIndexBuffer), NULL, GL_STREAM_DRAW);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(PostprocessVtxBuffer), PostprocessVtxBuffer, GL_STATIC_DRAW);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyVBOs()
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
	
	this->isVBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreatePBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenBuffers(1, &OGLRef.pboRenderDataID);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, OGLRef.pboRenderDataID);
	glBufferData(GL_PIXEL_PACK_BUFFER, this->_framebufferColorSizeBytes, NULL, GL_STREAM_READ);
	this->_mappedFramebuffer = (Color4u8 *__restrict)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
	
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
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		this->_mappedFramebuffer = NULL;
	}
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glDeleteBuffers(1, &this->ref->pboRenderDataID);
	
	this->isPBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreateVAOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenVertexArrays(1, &OGLRef.vaoGeometryStatesID);
	glGenVertexArrays(1, &OGLRef.vaoPostprocessStatesID);
	
	glBindVertexArray(OGLRef.vaoGeometryStatesID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
	
	if (this->isShaderSupported)
	{
		glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
		
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
		glEnableVertexAttribArray(OGLVertexAttributeID_Color);
		glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_INT, GL_FALSE, sizeof(NDSVertex), (const GLvoid *)offsetof(NDSVertex, position));
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_INT, GL_FALSE, sizeof(NDSVertex), (const GLvoid *)offsetof(NDSVertex, texCoord));
		glVertexAttribPointer(OGLVertexAttributeID_Color, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(NDSVertex), (const GLvoid *)offsetof(NDSVertex, color));
	}
#if !defined(GL_ES_VERSION_3_0)
	else
	{
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glVertexPointer(4, GL_FLOAT, 0, OGLRef.position4fBuffer);
		glTexCoordPointer(2, GL_FLOAT, 0, OGLRef.texCoord2fBuffer);
		glColorPointer(4, GL_FLOAT, 0, OGLRef.color4fBuffer);
	}
#endif
	
	glBindVertexArray(0);
	
	glBindVertexArray(OGLRef.vaoPostprocessStatesID);
	
	if (this->isShaderSupported)
	{
		glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
		
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
		glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(sizeof(GLfloat) * 8));
	}
	else
	{
		// Do nothing. Framebuffer post-processing requires shaders.
	}
	
	glBindVertexArray(0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyVAOs()
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

Render3DError OpenGLRenderer_1_2::CreateFBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up FBO render targets
	glGenTextures(1, &OGLRef.texCIColorID);
	glGenTextures(1, &OGLRef.texCIFogAttrID);
	glGenTextures(1, &OGLRef.texCIDepthStencilID);
	
	glGenTextures(1, &OGLRef.texGColorID);
	glGenTextures(1, &OGLRef.texGFogAttrID);
	glGenTextures(1, &OGLRef.texGPolyID);
	glGenTextures(1, &OGLRef.texGDepthStencilID);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_DepthStencil);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthStencilID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GColor);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GPolyID);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGPolyID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FogAttr);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGFogAttrID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	
	glActiveTexture(GL_TEXTURE0);
	
	CACHE_ALIGN GLint tempClearImageBuffer[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	memset(tempClearImageBuffer, 0, sizeof(tempClearImageBuffer));
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, OGL_TEXTURE_SRC_CI_COLOR, tempClearImageBuffer);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIDepthStencilID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, tempClearImageBuffer);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIFogAttrID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, OGL_TEXTURE_SRC_CI_FOG, tempClearImageBuffer);
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	// Set up FBOs
	glGenFramebuffersEXT(1, &OGLRef.fboClearImageID);
	glGenFramebuffersEXT(1, &OGLRef.fboFramebufferFlipID);
	glGenFramebuffersEXT(1, &OGLRef.fboRenderID);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboClearImageID);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, OGL_COLOROUT_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texCIColorID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, OGL_FOGATTRIBUTES_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texCIFogAttrID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texCIDepthStencilID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texCIDepthStencilID, 0);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to create FBOs!\n");
		this->DestroyFBOs();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	// Assign the default read/draw buffers.
	glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboFramebufferFlipID);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, OGL_COLOROUT_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texGColorID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, OGL_WORKING_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texFinalColorID, 0);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to create FBOs!\n");
		this->DestroyFBOs();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	// Assign the default read/draw buffers.
	glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, OGL_COLOROUT_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texGColorID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, OGL_POLYID_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texGPolyID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, OGL_FOGATTRIBUTES_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texGFogAttrID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, OGL_WORKING_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texFinalColorID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texGDepthStencilID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texGDepthStencilID, 0);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to create FBOs!\n");
		this->DestroyFBOs();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	// Assign the default read/draw buffers.
	glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	
	OGLRef.selectedRenderingFBO = OGLRef.fboRenderID;
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
	glDeleteFramebuffersEXT(1, &OGLRef.fboFramebufferFlipID);
	glDeleteFramebuffersEXT(1, &OGLRef.fboRenderID);
	glDeleteTextures(1, &OGLRef.texCIColorID);
	glDeleteTextures(1, &OGLRef.texCIFogAttrID);
	glDeleteTextures(1, &OGLRef.texCIDepthStencilID);
	glDeleteTextures(1, &OGLRef.texGColorID);
	glDeleteTextures(1, &OGLRef.texGPolyID);
	glDeleteTextures(1, &OGLRef.texGFogAttrID);
	glDeleteTextures(1, &OGLRef.texGDepthStencilID);
	
	OGLRef.fboClearImageID = 0;
	OGLRef.fboFramebufferFlipID = 0;
	OGLRef.fboRenderID = 0;
	OGLRef.texCIColorID = 0;
	OGLRef.texCIFogAttrID = 0;
	OGLRef.texCIDepthStencilID = 0;
	OGLRef.texGColorID = 0;
	OGLRef.texGPolyID = 0;
	OGLRef.texGFogAttrID = 0;
	OGLRef.texGDepthStencilID = 0;
	
	this->isFBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreateMultisampledFBO(GLsizei numSamples)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up FBO render targets
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGColorID);
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGWorkingID);
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGPolyID);
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGFogAttrID);
	glGenRenderbuffersEXT(1, &OGLRef.rboMSGDepthStencilID);
	
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGColorID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGWorkingID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGPolyID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGFogAttrID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_DEPTH24_STENCIL8_EXT, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
	
	// Set up multisampled rendering FBO
	glGenFramebuffersEXT(1, &OGLRef.fboMSIntermediateRenderID);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboMSIntermediateRenderID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, OGL_COLOROUT_ATTACHMENT_ID, GL_RENDERBUFFER_EXT, OGLRef.rboMSGColorID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, OGL_POLYID_ATTACHMENT_ID, GL_RENDERBUFFER_EXT, OGLRef.rboMSGPolyID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, OGL_FOGATTRIBUTES_ATTACHMENT_ID, GL_RENDERBUFFER_EXT, OGLRef.rboMSGFogAttrID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, OGL_WORKING_ATTACHMENT_ID, GL_RENDERBUFFER_EXT, OGLRef.rboMSGWorkingID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilID);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to create multisampled FBO!\n");
		this->DestroyMultisampledFBO();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	
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
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGWorkingID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGPolyID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGFogAttrID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMSGDepthStencilID);
	
	OGLRef.fboMSIntermediateRenderID = 0;
	OGLRef.rboMSGColorID = 0;
	OGLRef.rboMSGWorkingID = 0;
	OGLRef.rboMSGPolyID = 0;
	OGLRef.rboMSGFogAttrID = 0;
	OGLRef.rboMSGDepthStencilID = 0;
	
	this->isMultisampledFBOSupported = false;
}

void OpenGLRenderer_1_2::ResizeMultisampledFBOs(GLsizei numSamples)
{
	OGLRenderRef &OGLRef = *this->ref;
	GLsizei w = (GLsizei)this->_framebufferWidth;
	GLsizei h = (GLsizei)this->_framebufferHeight;
	
	if ( !this->isMultisampledFBOSupported ||
		 (numSamples == 1) ||
		 (w < GPU_FRAMEBUFFER_NATIVE_WIDTH) || (h < GPU_FRAMEBUFFER_NATIVE_HEIGHT) )
	{
		return;
	}
	
	if (numSamples == 0)
	{
		w = 0;
		h = 0;
		numSamples = 2;
	}
	
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGColorID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_RGBA8, w, h);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGWorkingID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_RGBA8, w, h);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGPolyID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_RGBA8, w, h);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGFogAttrID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_RGBA8, w, h);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMSGDepthStencilID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, numSamples, GL_DEPTH24_STENCIL8_EXT, w, h);
}

Render3DError OpenGLRenderer_1_2::CreateGeometryPrograms()
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenTextures(1, &OGLRef.texToonTableID);
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_LookupTable);
	glBindTexture(GL_TEXTURE_1D, OGLRef.texToonTableID);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 32, 0, GL_RGBA, OGL_TEXTURE_SRC_TOON_TABLE, NULL);
	
	glGenTextures(1, &OGLRef.texEdgeColorTableID);
	glBindTexture(GL_TEXTURE_1D, OGLRef.texEdgeColorTableID);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 8, 0, GL_RGBA, OGL_TEXTURE_SRC_EDGE_COLOR, NULL);
	
	glGenTextures(1, &OGLRef.texFogDensityTableID);
	glBindTexture(GL_TEXTURE_1D, OGLRef.texFogDensityTableID);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_LUMINANCE, 32, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glActiveTexture(GL_TEXTURE0);
	
	OGLGeometryFlags programFlags;
	programFlags.value = 0;
	
	std::stringstream vtxShaderHeader;
	vtxShaderHeader << "#define DEPTH_EQUALS_TEST_TOLERANCE " << DEPTH_EQUALS_TEST_TOLERANCE << ".0\n";
	vtxShaderHeader << "\n";
	
	std::string vtxShaderCode  = vtxShaderHeader.str() + std::string(GeometryVtxShader_100);
	
	std::stringstream fragShaderHeader;
	fragShaderHeader << "#define FRAMEBUFFER_SIZE_X " << this->_framebufferWidth  << ".0 \n";
	fragShaderHeader << "#define FRAMEBUFFER_SIZE_Y " << this->_framebufferHeight << ".0 \n";
	fragShaderHeader << "\n";
	fragShaderHeader << "#define OUTFRAGCOLOR " << ((this->isFBOSupported) ? "gl_FragData[0]" : "gl_FragColor") << "\n";
	fragShaderHeader << "\n";
	
	for (size_t flagsValue = 0; flagsValue < 128; flagsValue++, programFlags.value++)
	{
		std::stringstream shaderFlags;
		shaderFlags << "#define USE_TEXTURE_SMOOTHING " << ((this->_enableTextureSmoothing) ? 1 : 0) << "\n";
		shaderFlags << "#define USE_NDS_DEPTH_CALCULATION " << ((this->_emulateNDSDepthCalculation) ? 1 : 0) << "\n";
		shaderFlags << "#define USE_DEPTH_LEQUAL_POLYGON_FACING " << ((this->_emulateDepthLEqualPolygonFacing && this->isVBOSupported && this->isFBOSupported) ? 1 : 0) << "\n";
		shaderFlags << "\n";
		shaderFlags << "#define ENABLE_W_DEPTH " << ((programFlags.EnableWDepth) ? 1 : 0) << "\n";
		shaderFlags << "#define ENABLE_ALPHA_TEST " << ((programFlags.EnableAlphaTest) ? "true\n" : "false\n");
		shaderFlags << "#define ENABLE_TEXTURE_SAMPLING " << ((programFlags.EnableTextureSampling) ? "true\n" : "false\n");
		shaderFlags << "#define TOON_SHADING_MODE " << ((programFlags.ToonShadingMode) ? 1 : 0) << "\n";
		shaderFlags << "#define ENABLE_FOG " << ((programFlags.EnableFog && this->isVBOSupported && this->isFBOSupported) ? 1 : 0) << "\n";
		shaderFlags << "#define ENABLE_EDGE_MARK " << ((programFlags.EnableEdgeMark && this->isVBOSupported && this->isFBOSupported) ? 1 : 0) << "\n";
		shaderFlags << "#define DRAW_MODE_OPAQUE " << ((programFlags.OpaqueDrawMode && this->isVBOSupported && this->isFBOSupported) ? 1 : 0) << "\n";
		shaderFlags << "\n";
		shaderFlags << "#define ATTACHMENT_WORKING_BUFFER " << GeometryAttachmentWorkingBuffer[programFlags.DrawBuffersMode] << "\n";
		shaderFlags << "#define ATTACHMENT_POLY_ID " << GeometryAttachmentPolyID[programFlags.DrawBuffersMode] << "\n";
		shaderFlags << "#define ATTACHMENT_FOG_ATTRIBUTES " << GeometryAttachmentFogAttributes[programFlags.DrawBuffersMode] << "\n";
		shaderFlags << "\n";
		
		std::string fragShaderCode = fragShaderHeader.str() + shaderFlags.str() + std::string(GeometryFragShader_100);
		
		error = this->ShaderProgramCreate(OGLRef.vertexGeometryShaderID,
										  OGLRef.fragmentGeometryShaderID[flagsValue],
										  OGLRef.programGeometryID[flagsValue],
										  vtxShaderCode.c_str(),
										  fragShaderCode.c_str());
		if (error != OGLERROR_NOERR)
		{
			INFO("OpenGL: Failed to create the GEOMETRY shader program.\n");
			glUseProgram(0);
			this->DestroyGeometryPrograms();
			return error;
		}
		
		glBindAttribLocation(OGLRef.programGeometryID[flagsValue], OGLVertexAttributeID_Position, "inPosition");
		glBindAttribLocation(OGLRef.programGeometryID[flagsValue], OGLVertexAttributeID_TexCoord0, "inTexCoord0");
		glBindAttribLocation(OGLRef.programGeometryID[flagsValue], OGLVertexAttributeID_Color, "inColor");
		
		glLinkProgram(OGLRef.programGeometryID[flagsValue]);
		if (!this->ValidateShaderProgramLink(OGLRef.programGeometryID[flagsValue]))
		{
			INFO("OpenGL: Failed to link the GEOMETRY shader program.\n");
			glUseProgram(0);
			this->DestroyGeometryPrograms();
			return OGLERROR_SHADER_CREATE_ERROR;
		}
		
		glValidateProgram(OGLRef.programGeometryID[flagsValue]);
		glUseProgram(OGLRef.programGeometryID[flagsValue]);
		
		const GLint uniformTexRenderObject						= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "texRenderObject");
		glUniform1i(uniformTexRenderObject, 0);
		
		const GLint uniformTexToonTable							= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "texToonTable");
		glUniform1i(uniformTexToonTable, OGLTextureUnitID_LookupTable);
		
		if (this->_emulateDepthLEqualPolygonFacing && this->_isDepthLEqualPolygonFacingSupported && !programFlags.OpaqueDrawMode)
		{
			const GLint uniformTexBackfacing					= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "inDstBackFacing");
			glUniform1i(uniformTexBackfacing, OGLTextureUnitID_FinalColor);
		}
		
		OGLRef.uniformStateAlphaTestRef[flagsValue]				= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "stateAlphaTestRef");
		
		OGLRef.uniformPolyTexScale[flagsValue]					= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyTexScale");
		OGLRef.uniformPolyMode[flagsValue]						= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyMode");
		OGLRef.uniformPolyIsWireframe[flagsValue]				= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyIsWireframe");
		OGLRef.uniformPolySetNewDepthForTranslucent[flagsValue]	= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polySetNewDepthForTranslucent");
		OGLRef.uniformPolyAlpha[flagsValue]						= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyAlpha");
		OGLRef.uniformPolyID[flagsValue]						= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyID");
		
		OGLRef.uniformPolyEnableTexture[flagsValue]				= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyEnableTexture");
		OGLRef.uniformPolyEnableFog[flagsValue]					= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyEnableFog");
		OGLRef.uniformTexSingleBitAlpha[flagsValue]				= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "texSingleBitAlpha");
		
		OGLRef.uniformTexDrawOpaque[flagsValue]					= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "texDrawOpaque");
		OGLRef.uniformDrawModeDepthEqualsTest[flagsValue]		= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "drawModeDepthEqualsTest");
		OGLRef.uniformPolyIsBackFacing[flagsValue]              = glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyIsBackFacing");
		OGLRef.uniformPolyDrawShadow[flagsValue]				= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyDrawShadow");
		OGLRef.uniformPolyDepthOffset[flagsValue]				= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyDepthOffset");
	}
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyGeometryPrograms()
{
	if (!this->isShaderSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	for (size_t flagsValue = 0; flagsValue < 128; flagsValue++)
	{
		if (OGLRef.programGeometryID[flagsValue] == 0)
		{
			continue;
		}
		
		glDetachShader(OGLRef.programGeometryID[flagsValue], OGLRef.vertexGeometryShaderID);
		glDetachShader(OGLRef.programGeometryID[flagsValue], OGLRef.fragmentGeometryShaderID[flagsValue]);
		glDeleteProgram(OGLRef.programGeometryID[flagsValue]);
		glDeleteShader(OGLRef.fragmentGeometryShaderID[flagsValue]);
		
		OGLRef.programGeometryID[flagsValue] = 0;
		OGLRef.fragmentGeometryShaderID[flagsValue] = 0;
	}
	
	glDeleteShader(OGLRef.vertexGeometryShaderID);
	OGLRef.vertexGeometryShaderID = 0;
	
	glDeleteTextures(1, &ref->texToonTableID);
	OGLRef.texToonTableID = 0;
	
	glDeleteTextures(1, &ref->texEdgeColorTableID);
	OGLRef.texEdgeColorTableID = 0;
	
	glDeleteTextures(1, &ref->texFogDensityTableID);
	OGLRef.texFogDensityTableID = 0;
}

Render3DError OpenGLRenderer_1_2::CreateGeometryZeroDstAlphaProgram(const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (vtxShaderCString == NULL) || (fragShaderCString == NULL) )
	{
		return error;
	}
	
	error = this->ShaderProgramCreate(OGLRef.vtxShaderGeometryZeroDstAlphaID,
									  OGLRef.fragShaderGeometryZeroDstAlphaID,
									  OGLRef.programGeometryZeroDstAlphaID,
									  vtxShaderCString,
									  fragShaderCString);
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL: Failed to create the GEOMETRY ZERO DST ALPHA shader program.\n");
		glUseProgram(0);
		this->DestroyGeometryZeroDstAlphaProgram();
		return error;
	}
	
	glBindAttribLocation(OGLRef.programGeometryZeroDstAlphaID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programGeometryZeroDstAlphaID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	
	glLinkProgram(OGLRef.programGeometryZeroDstAlphaID);
	if (!this->ValidateShaderProgramLink(OGLRef.programGeometryZeroDstAlphaID))
	{
		INFO("OpenGL: Failed to link the GEOMETRY ZERO DST ALPHA shader program.\n");
		glUseProgram(0);
		this->DestroyGeometryZeroDstAlphaProgram();
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.programGeometryZeroDstAlphaID);
	glUseProgram(OGLRef.programGeometryZeroDstAlphaID);
	
	const GLint uniformTexGColor = glGetUniformLocation(OGLRef.programGeometryZeroDstAlphaID, "texInFragColor");
	glUniform1i(uniformTexGColor, OGLTextureUnitID_GColor);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyGeometryZeroDstAlphaProgram()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->isShaderSupported || (OGLRef.programGeometryZeroDstAlphaID == 0))
	{
		return;
	}
	
	glDetachShader(OGLRef.programGeometryZeroDstAlphaID, OGLRef.vtxShaderGeometryZeroDstAlphaID);
	glDetachShader(OGLRef.programGeometryZeroDstAlphaID, OGLRef.fragShaderGeometryZeroDstAlphaID);
	glDeleteProgram(OGLRef.programGeometryZeroDstAlphaID);
	glDeleteShader(OGLRef.vtxShaderGeometryZeroDstAlphaID);
	glDeleteShader(OGLRef.fragShaderGeometryZeroDstAlphaID);
	
	OGLRef.programGeometryZeroDstAlphaID = 0;
	OGLRef.vtxShaderGeometryZeroDstAlphaID = 0;
	OGLRef.fragShaderGeometryZeroDstAlphaID = 0;
}

Render3DError OpenGLRenderer_1_2::CreateEdgeMarkProgram(const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (vtxShaderCString == NULL) || (fragShaderCString == NULL) )
	{
		return error;
	}
	
	std::stringstream shaderHeader;
	shaderHeader << "#define FRAMEBUFFER_SIZE_X " << this->_framebufferWidth  << ".0 \n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_Y " << this->_framebufferHeight << ".0 \n";
	shaderHeader << "\n";
	
	std::string vtxShaderCode  = shaderHeader.str() + std::string(vtxShaderCString);
	std::string fragShaderCode = shaderHeader.str() + std::string(fragShaderCString);
	
	error = this->ShaderProgramCreate(OGLRef.vertexEdgeMarkShaderID,
									  OGLRef.fragmentEdgeMarkShaderID,
									  OGLRef.programEdgeMarkID,
									  vtxShaderCode.c_str(),
									  fragShaderCode.c_str());
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL: Failed to create the EDGE MARK shader program.\n");
		glUseProgram(0);
		this->DestroyEdgeMarkProgram();
		return error;
	}
	
	glBindAttribLocation(OGLRef.programEdgeMarkID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programEdgeMarkID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	
	glLinkProgram(OGLRef.programEdgeMarkID);
	if (!this->ValidateShaderProgramLink(OGLRef.programEdgeMarkID))
	{
		INFO("OpenGL: Failed to link the EDGE MARK shader program.\n");
		glUseProgram(0);
		this->DestroyEdgeMarkProgram();
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.programEdgeMarkID);
	glUseProgram(OGLRef.programEdgeMarkID);
	
	const GLint uniformTexGDepth			= glGetUniformLocation(OGLRef.programEdgeMarkID, "texInFragDepth");
	const GLint uniformTexGPolyID			= glGetUniformLocation(OGLRef.programEdgeMarkID, "texInPolyID");
	const GLint uniformTexEdgeColorTable	= glGetUniformLocation(OGLRef.programEdgeMarkID, "texEdgeColor");
	glUniform1i(uniformTexGDepth, OGLTextureUnitID_DepthStencil);
	glUniform1i(uniformTexGPolyID, OGLTextureUnitID_GPolyID);
	glUniform1i(uniformTexEdgeColorTable, OGLTextureUnitID_LookupTable);
	
	OGLRef.uniformStateClearPolyID			= glGetUniformLocation(OGLRef.programEdgeMarkID, "clearPolyID");
	OGLRef.uniformStateClearDepth			= glGetUniformLocation(OGLRef.programEdgeMarkID, "clearDepth");
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyEdgeMarkProgram()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->isShaderSupported || (OGLRef.programEdgeMarkID == 0))
	{
		return;
	}
	
	glDetachShader(OGLRef.programEdgeMarkID, OGLRef.vertexEdgeMarkShaderID);
	glDetachShader(OGLRef.programEdgeMarkID, OGLRef.fragmentEdgeMarkShaderID);
	glDeleteProgram(OGLRef.programEdgeMarkID);
	glDeleteShader(OGLRef.vertexEdgeMarkShaderID);
	glDeleteShader(OGLRef.fragmentEdgeMarkShaderID);
	
	OGLRef.programEdgeMarkID = 0;
	OGLRef.vertexEdgeMarkShaderID = 0;
	OGLRef.fragmentEdgeMarkShaderID = 0;
}

Render3DError OpenGLRenderer_1_2::CreateFogProgram(const OGLFogProgramKey fogProgramKey, const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if (vtxShaderCString == NULL)
	{
		INFO("OpenGL: The FOG vertex shader is unavailable.\n");
		error = OGLERROR_VERTEX_SHADER_PROGRAM_LOAD_ERROR;
		return error;
	}
	else if (fragShaderCString == NULL)
	{
		INFO("OpenGL: The FOG fragment shader is unavailable.\n");
		error = OGLERROR_FRAGMENT_SHADER_PROGRAM_LOAD_ERROR;
		return error;
	}
	
	const s32 fogOffset = fogProgramKey.offset;
	const GLfloat fogOffsetf = (GLfloat)fogOffset / 32767.0f;
	const s32 fogStep = 0x0400 >> fogProgramKey.shift;
	
	std::stringstream fragDepthConstants;
	fragDepthConstants << "#define FOG_OFFSET " << fogOffset << "\n";
	fragDepthConstants << "#define FOG_OFFSETF " << fogOffsetf << (((fogOffsetf == 0.0f) || (fogOffsetf == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_STEP " << fogStep << "\n";
	fragDepthConstants << "\n";
	
	std::string fragShaderCode = fragDepthConstants.str() + std::string(fragShaderCString);
	
	OGLFogShaderID shaderID;
	shaderID.program = 0;
	shaderID.fragShader = 0;
	
	error = this->ShaderProgramCreate(OGLRef.vertexFogShaderID,
									  shaderID.fragShader,
									  shaderID.program,
									  vtxShaderCString,
									  fragShaderCode.c_str());
	
	this->_fogProgramMap[fogProgramKey.key] = shaderID;
	
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL: Failed to create the FOG shader program.\n");
		glUseProgram(0);
		this->DestroyFogProgram(fogProgramKey);
		return error;
	}
	
	glBindAttribLocation(shaderID.program, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(shaderID.program, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	
	glLinkProgram(shaderID.program);
	if (!this->ValidateShaderProgramLink(shaderID.program))
	{
		INFO("OpenGL: Failed to link the FOG shader program.\n");
		glUseProgram(0);
		this->DestroyFogProgram(fogProgramKey);
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(shaderID.program);
	glUseProgram(shaderID.program);
	
	const GLint uniformTexGColor          = glGetUniformLocation(shaderID.program, "texInFragColor");
	const GLint uniformTexGDepth          = glGetUniformLocation(shaderID.program, "texInFragDepth");
	const GLint uniformTexGFog            = glGetUniformLocation(shaderID.program, "texInFogAttributes");
	const GLint uniformTexFogDensityTable = glGetUniformLocation(shaderID.program, "texFogDensityTable");
	glUniform1i(uniformTexGColor, OGLTextureUnitID_GColor);
	glUniform1i(uniformTexGDepth, OGLTextureUnitID_DepthStencil);
	glUniform1i(uniformTexGFog, OGLTextureUnitID_FogAttr);
	glUniform1i(uniformTexFogDensityTable, OGLTextureUnitID_LookupTable);
	
	OGLRef.uniformStateEnableFogAlphaOnly = glGetUniformLocation(shaderID.program, "stateEnableFogAlphaOnly");
	OGLRef.uniformStateFogColor           = glGetUniformLocation(shaderID.program, "stateFogColor");
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyFogProgram(const OGLFogProgramKey fogProgramKey)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->isShaderSupported)
	{
		return;
	}
	
	std::map<u32, OGLFogShaderID>::iterator it = this->_fogProgramMap.find(fogProgramKey.key);
	if (it == this->_fogProgramMap.end())
	{
		return;
	}
	
	OGLFogShaderID shaderID = this->_fogProgramMap[fogProgramKey.key];
	glDetachShader(shaderID.program, OGLRef.vertexFogShaderID);
	glDetachShader(shaderID.program, shaderID.fragShader);
	glDeleteProgram(shaderID.program);
	glDeleteShader(shaderID.fragShader);
	
	this->_fogProgramMap.erase(it);
	
	if (this->_fogProgramMap.size() == 0)
	{
		glDeleteShader(OGLRef.vertexFogShaderID);
		OGLRef.vertexFogShaderID = 0;
	}
}

void OpenGLRenderer_1_2::DestroyFogPrograms()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->isShaderSupported)
	{
		return;
	}
	
	while (this->_fogProgramMap.size() > 0)
	{
		std::map<u32, OGLFogShaderID>::iterator it = this->_fogProgramMap.begin();
		OGLFogShaderID shaderID = it->second;
		
		glDetachShader(shaderID.program, OGLRef.vertexFogShaderID);
		glDetachShader(shaderID.program, shaderID.fragShader);
		glDeleteProgram(shaderID.program);
		glDeleteShader(shaderID.fragShader);
		
		this->_fogProgramMap.erase(it);
		
		if (this->_fogProgramMap.size() == 0)
		{
			glDeleteShader(OGLRef.vertexFogShaderID);
			OGLRef.vertexFogShaderID = 0;
		}
	}
}

Render3DError OpenGLRenderer_1_2::CreateFramebufferOutput6665Program(const size_t outColorIndex, const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (vtxShaderCString == NULL) || (fragShaderCString == NULL) )
	{
		return error;
	}
	
	std::stringstream shaderHeader;
	shaderHeader << "#define FRAMEBUFFER_SIZE_X " << this->_framebufferWidth  << ".0 \n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_Y " << this->_framebufferHeight << ".0 \n";
	shaderHeader << "\n";
	
	std::string vtxShaderCode  = shaderHeader.str() + std::string(vtxShaderCString);
	
	error = this->ShaderProgramCreate(OGLRef.vertexFramebufferOutput6665ShaderID,
									  OGLRef.fragmentFramebufferRGBA6665OutputShaderID,
									  OGLRef.programFramebufferRGBA6665OutputID[outColorIndex],
									  vtxShaderCode.c_str(),
									  fragShaderCString);
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL: Failed to create the FRAMEBUFFER OUTPUT RGBA6665 shader program.\n");
		glUseProgram(0);
		this->DestroyFramebufferOutput6665Programs();
		return error;
	}
	
	glBindAttribLocation(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex], OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex], OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	
	glLinkProgram(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex]);
	if (!this->ValidateShaderProgramLink(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex]))
	{
		INFO("OpenGL: Failed to link the FRAMEBUFFER OUTPUT RGBA6665 shader program.\n");
		glUseProgram(0);
		this->DestroyFramebufferOutput6665Programs();
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex]);
	glUseProgram(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex]);
	
	const GLint uniformTexGColor = glGetUniformLocation(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex], "texInFragColor");
	if (outColorIndex == 0)
	{
		glUniform1i(uniformTexGColor, OGLTextureUnitID_FinalColor);
	}
	else
	{
		glUniform1i(uniformTexGColor, OGLTextureUnitID_GColor);
	}
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyFramebufferOutput6665Programs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->isShaderSupported)
	{
		return;
	}
	
	if (OGLRef.programFramebufferRGBA6665OutputID[0] != 0)
	{
		glDetachShader(OGLRef.programFramebufferRGBA6665OutputID[0], OGLRef.vertexFramebufferOutput6665ShaderID);
		glDetachShader(OGLRef.programFramebufferRGBA6665OutputID[0], OGLRef.fragmentFramebufferRGBA6665OutputShaderID);
		glDeleteProgram(OGLRef.programFramebufferRGBA6665OutputID[0]);
		OGLRef.programFramebufferRGBA6665OutputID[0] = 0;
	}
	
	if (OGLRef.programFramebufferRGBA6665OutputID[1] != 0)
	{
		glDetachShader(OGLRef.programFramebufferRGBA6665OutputID[1], OGLRef.vertexFramebufferOutput6665ShaderID);
		glDetachShader(OGLRef.programFramebufferRGBA6665OutputID[1], OGLRef.fragmentFramebufferRGBA6665OutputShaderID);
		glDeleteProgram(OGLRef.programFramebufferRGBA6665OutputID[1]);
		OGLRef.programFramebufferRGBA6665OutputID[1] = 0;
	}
	
	glDeleteShader(OGLRef.vertexFramebufferOutput6665ShaderID);
	glDeleteShader(OGLRef.fragmentFramebufferRGBA6665OutputShaderID);
	OGLRef.vertexFramebufferOutput6665ShaderID = 0;
	OGLRef.fragmentFramebufferRGBA6665OutputShaderID = 0;
}

Render3DError OpenGLRenderer_1_2::CreateFramebufferOutput8888Program(const size_t outColorIndex, const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (vtxShaderCString == NULL) || (fragShaderCString == NULL) )
	{
		return error;
	}
	
	std::stringstream shaderHeader;
	shaderHeader << "#define FRAMEBUFFER_SIZE_X " << this->_framebufferWidth  << ".0 \n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_Y " << this->_framebufferHeight << ".0 \n";
	shaderHeader << "\n";
	
	std::string vtxShaderCode  = shaderHeader.str() + std::string(vtxShaderCString);
	
	error = this->ShaderProgramCreate(OGLRef.vertexFramebufferOutput8888ShaderID,
									  OGLRef.fragmentFramebufferRGBA8888OutputShaderID,
									  OGLRef.programFramebufferRGBA8888OutputID[outColorIndex],
									  vtxShaderCode.c_str(),
									  fragShaderCString);
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL: Failed to create the FRAMEBUFFER OUTPUT RGBA8888 shader program.\n");
		glUseProgram(0);
		this->DestroyFramebufferOutput8888Programs();
		return error;
	}
	
	glBindAttribLocation(OGLRef.programFramebufferRGBA8888OutputID[outColorIndex], OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programFramebufferRGBA8888OutputID[outColorIndex], OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	
	glLinkProgram(OGLRef.programFramebufferRGBA8888OutputID[outColorIndex]);
	if (!this->ValidateShaderProgramLink(OGLRef.programFramebufferRGBA8888OutputID[outColorIndex]))
	{
		INFO("OpenGL: Failed to link the FRAMEBUFFER OUTPUT RGBA8888 shader program.\n");
		glUseProgram(0);
		this->DestroyFramebufferOutput8888Programs();
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.programFramebufferRGBA8888OutputID[outColorIndex]);
	glUseProgram(OGLRef.programFramebufferRGBA8888OutputID[outColorIndex]);
	
	const GLint uniformTexGColor = glGetUniformLocation(OGLRef.programFramebufferRGBA8888OutputID[outColorIndex], "texInFragColor");
	if (outColorIndex == 0)
	{
		glUniform1i(uniformTexGColor, OGLTextureUnitID_FinalColor);
	}
	else
	{
		glUniform1i(uniformTexGColor, OGLTextureUnitID_GColor);
	}
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyFramebufferOutput8888Programs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->isShaderSupported)
	{
		return;
	}
	
	if (OGLRef.programFramebufferRGBA8888OutputID[0] != 0)
	{
		glDetachShader(OGLRef.programFramebufferRGBA8888OutputID[0], OGLRef.vertexFramebufferOutput8888ShaderID);
		glDetachShader(OGLRef.programFramebufferRGBA8888OutputID[0], OGLRef.fragmentFramebufferRGBA8888OutputShaderID);
		glDeleteProgram(OGLRef.programFramebufferRGBA8888OutputID[0]);
		OGLRef.programFramebufferRGBA8888OutputID[0] = 0;
	}
	
	if (OGLRef.programFramebufferRGBA8888OutputID[1] != 0)
	{
		glDetachShader(OGLRef.programFramebufferRGBA8888OutputID[1], OGLRef.vertexFramebufferOutput8888ShaderID);
		glDetachShader(OGLRef.programFramebufferRGBA8888OutputID[1], OGLRef.fragmentFramebufferRGBA8888OutputShaderID);
		glDeleteProgram(OGLRef.programFramebufferRGBA8888OutputID[1]);
		OGLRef.programFramebufferRGBA8888OutputID[1] = 0;
	}
	
	glDeleteShader(OGLRef.vertexFramebufferOutput8888ShaderID);
	glDeleteShader(OGLRef.fragmentFramebufferRGBA8888OutputShaderID);
	OGLRef.vertexFramebufferOutput8888ShaderID = 0;
	OGLRef.fragmentFramebufferRGBA8888OutputShaderID = 0;
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
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_DST_ALPHA);
			glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);
		}
		else
		{
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_DST_ALPHA);
		}
	}
	else
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	
	// Mirrored Repeat Mode Support
	OGLRef.stateTexMirroredRepeat = (isTexMirroredRepeatSupported) ? GL_MIRRORED_REPEAT : GL_REPEAT;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitPostprocessingPrograms(const char *edgeMarkVtxShaderCString,
															 const char *edgeMarkFragShaderCString,
															 const char *framebufferOutputVtxShaderCString,
															 const char *framebufferOutputRGBA6665FragShaderCString,
															 const char *framebufferOutputRGBA8888FragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isVBOSupported && this->isFBOSupported)
	{
		error = this->CreateEdgeMarkProgram(edgeMarkVtxShaderCString, edgeMarkFragShaderCString);
		if (error != OGLERROR_NOERR)
		{
			return error;
		}
	}
	
	error = this->CreateFramebufferOutput6665Program(0, framebufferOutputVtxShaderCString, framebufferOutputRGBA6665FragShaderCString);
	if (error != OGLERROR_NOERR)
	{
		return error;
	}
	
	error = this->CreateFramebufferOutput6665Program(1, framebufferOutputVtxShaderCString, framebufferOutputRGBA6665FragShaderCString);
	if (error != OGLERROR_NOERR)
	{
		return error;
	}
	
	error = this->CreateFramebufferOutput8888Program(0, framebufferOutputVtxShaderCString, framebufferOutputRGBA8888FragShaderCString);
	if (error != OGLERROR_NOERR)
	{
		return error;
	}
	
	error = this->CreateFramebufferOutput8888Program(1, framebufferOutputVtxShaderCString, framebufferOutputRGBA8888FragShaderCString);
	if (error != OGLERROR_NOERR)
	{
		return error;
	}
	
	glUseProgram(OGLRef.programGeometryID[0]);
	INFO("OpenGL: Successfully created postprocess shaders.\n");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::UploadClearImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID)
{
	OGLRenderRef &OGLRef = *this->ref;
	this->_clearImageIndex ^= 0x01;
	
	if (this->_enableFog && this->_deviceInfo.isFogSupported)
	{
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT; i++)
		{
			OGLRef.workingCIDepthStencilBuffer[this->_clearImageIndex][i] = (depthBuffer[i] << 8) | opaquePolyID;
			OGLRef.workingCIFogAttributesBuffer[this->_clearImageIndex][i] = (fogBuffer[i]) ? 0xFF0000FF : 0xFF000000;
		}
	}
	else
	{
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT; i++)
		{
			OGLRef.workingCIDepthStencilBuffer[this->_clearImageIndex][i] = (depthBuffer[i] << 8) | opaquePolyID;
		}
	}
	
	const bool didColorChange = (memcmp(OGLRef.workingCIColorBuffer16, colorBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16)) != 0);
	const bool didDepthStencilChange = (memcmp(OGLRef.workingCIDepthStencilBuffer[this->_clearImageIndex], OGLRef.workingCIDepthStencilBuffer[this->_clearImageIndex ^ 0x01], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(GLuint)) != 0);
	const bool didFogAttributesChange = this->_enableFog && this->_deviceInfo.isFogSupported && (memcmp(OGLRef.workingCIFogAttributesBuffer[this->_clearImageIndex], OGLRef.workingCIFogAttributesBuffer[this->_clearImageIndex ^ 0x01], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(GLuint)) != 0);
	
	glActiveTexture(GL_TEXTURE0);
	
	if (didColorChange)
	{
		memcpy(OGLRef.workingCIColorBuffer16, colorBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
		glBindTexture(GL_TEXTURE_2D, OGLRef.texCIColorID);
		
		if (OGL_TEXTURE_SRC_CI_COLOR == GL_UNSIGNED_BYTE)
		{
			ColorspaceConvertBuffer5551To8888<false, false, BESwapDst>(OGLRef.workingCIColorBuffer16, OGLRef.workingCIColorBuffer32, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GL_RGBA, OGL_TEXTURE_SRC_CI_COLOR, OGLRef.workingCIColorBuffer32);
		}
		else
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GL_RGBA, OGL_TEXTURE_SRC_CI_COLOR, OGLRef.workingCIColorBuffer16);
		}
	}
	
	if (didDepthStencilChange)
	{
		glBindTexture(GL_TEXTURE_2D, OGLRef.texCIDepthStencilID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, OGLRef.workingCIDepthStencilBuffer[this->_clearImageIndex]);
	}
	
	if (didFogAttributesChange)
	{
		glBindTexture(GL_TEXTURE_2D, OGLRef.texCIFogAttrID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GL_RGBA, OGL_TEXTURE_SRC_CI_FOG, OGLRef.workingCIFogAttributesBuffer[this->_clearImageIndex]);
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

void OpenGLRenderer_1_2::_SetupGeometryShaders(const OGLGeometryFlags flags)
{
	const OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->isShaderSupported)
	{
		if (this->isFBOSupported)
		{
			glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
		}
		return;
	}
	
	glUseProgram(OGLRef.programGeometryID[flags.value]);
	glUniform1f(OGLRef.uniformStateAlphaTestRef[flags.value], this->_pendingRenderStates.alphaTestRef);
	glUniform1i(OGLRef.uniformTexDrawOpaque[flags.value], GL_FALSE);
	glUniform1i(OGLRef.uniformDrawModeDepthEqualsTest[flags.value], GL_FALSE);
	glUniform1i(OGLRef.uniformPolyDrawShadow[flags.value], GL_FALSE);
	
	if (this->isFBOSupported)
	{
		glDrawBuffers(4, GeometryDrawBuffersEnum[flags.DrawBuffersMode]);
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
			glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_INT, GL_FALSE, sizeof(NDSVertex), (const GLvoid *)offsetof(NDSVertex, position));
			glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_INT, GL_FALSE, sizeof(NDSVertex), (const GLvoid *)offsetof(NDSVertex, texCoord));
			glVertexAttribPointer(OGLVertexAttributeID_Color, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(NDSVertex), (const GLvoid *)offsetof(NDSVertex, color));
		}
#if !defined(GL_ES_VERSION_3_0)
		else
		{
			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glVertexPointer(4, GL_FLOAT, 0, OGLRef.position4fBuffer);
			glTexCoordPointer(2, GL_FLOAT, 0, OGLRef.texCoord2fBuffer);
			glColorPointer(4, GL_FLOAT, 0, OGLRef.color4fBuffer);
		}
#endif
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
#if !defined(GL_ES_VERSION_3_0)
		else
		{
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
		}
#endif
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ZeroDstAlphaPass(const POLY *rawPolyList, const CPoly *clippedPolyList, const size_t clippedPolyCount, const size_t clippedPolyOpaqueCount, bool enableAlphaBlending, size_t indexOffset, POLYGON_ATTR lastPolyAttr)
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
		// This is not perfectly pixel accurate, but it's better than nothing.
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
		glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
		glBlitFramebufferEXT(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glDrawBuffers(4, GeometryDrawBuffersEnum[this->_geometryProgramFlags.DrawBuffersMode]);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
	}
	
	glUseProgram(OGLRef.programGeometryZeroDstAlphaID);
	glViewport(0, 0, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
	glDisable(GL_BLEND);
	glEnable(GL_STENCIL_TEST);
	glDisable(GL_DEPTH_TEST);
	
	glStencilFunc(GL_ALWAYS, 0x40, 0x40);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilMask(0x40);
	glDepthMask(GL_FALSE);
	glDrawBuffer(GL_NONE);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
	
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
	
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
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
	OGLGeometryFlags oldGProgramFlags = this->_geometryProgramFlags;
	this->_geometryProgramFlags.EnableEdgeMark = 0;
	this->_geometryProgramFlags.EnableFog = 0;
	this->_SetupGeometryShaders(this->_geometryProgramFlags);
	glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
	this->EnableVertexAttributes();
	
	// Draw the alpha polys, touching fully transparent pixels only once.
	glEnable(GL_DEPTH_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	glStencilFunc(GL_NOTEQUAL, 0x40, 0x40);
	
	this->DrawPolygonsForIndexRange<OGLPolyDrawMode_ZeroAlphaPass>(rawPolyList, clippedPolyList, clippedPolyCount, clippedPolyOpaqueCount, clippedPolyCount - 1, indexOffset, lastPolyAttr);
	
	// Restore OpenGL states back to normal.
	this->_geometryProgramFlags = oldGProgramFlags;
	this->_SetupGeometryShaders(this->_geometryProgramFlags);
	glClear(GL_STENCIL_BUFFER_BIT);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	glStencilMask(0xFF);
	
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

void OpenGLRenderer_1_2::_ResolveWorkingBackFacing()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->_emulateDepthLEqualPolygonFacing || !this->_isDepthLEqualPolygonFacingSupported || !this->isMultisampledFBOSupported || (OGLRef.selectedRenderingFBO != OGLRef.fboMSIntermediateRenderID))
	{
		return;
	}
	
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboMSIntermediateRenderID);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
	
	glReadBuffer(OGL_WORKING_ATTACHMENT_ID);
	glDrawBuffer(OGL_WORKING_ATTACHMENT_ID);
	glBlitFramebufferEXT(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	
	// Reset framebuffer targets
	glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	glDrawBuffers(4, GeometryDrawBuffersEnum[this->_geometryProgramFlags.DrawBuffersMode]);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboMSIntermediateRenderID);
	glDrawBuffers(4, GeometryDrawBuffersEnum[this->_geometryProgramFlags.DrawBuffersMode]);
}

void OpenGLRenderer_1_2::_ResolveGeometry()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->isMultisampledFBOSupported || (OGLRef.selectedRenderingFBO != OGLRef.fboMSIntermediateRenderID))
	{
		return;
	}
	
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboMSIntermediateRenderID);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
	
	if (this->isShaderSupported)
	{
		if (this->_enableEdgeMark && this->_deviceInfo.isEdgeMarkSupported)
		{
			glReadBuffer(OGL_POLYID_ATTACHMENT_ID);
			glDrawBuffer(OGL_POLYID_ATTACHMENT_ID);
			glBlitFramebufferEXT(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		
		if (this->_enableFog && this->_deviceInfo.isFogSupported)
		{
			glReadBuffer(OGL_FOGATTRIBUTES_ATTACHMENT_ID);
			glDrawBuffer(OGL_FOGATTRIBUTES_ATTACHMENT_ID);
			glBlitFramebufferEXT(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		
		// Blit the color and depth buffers
		glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
		glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
		glBlitFramebufferEXT(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		
		// Reset framebuffer targets
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
		glDrawBuffers(4, GeometryDrawBuffersEnum[this->_geometryProgramFlags.DrawBuffersMode]);
	}
	else
	{
		// Blit the color buffer
		glBlitFramebufferEXT(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
		glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
		glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	}
}

Render3DError OpenGLRenderer_1_2::ReadBackPixels()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->willFlipAndConvertFramebufferOnGPU)
	{
		// Both flips and converts the framebuffer on the GPU. No additional postprocessing
		// should be necessary at this point.
		if (this->isFBOSupported)
		{
			if (this->_lastTextureDrawTarget == OGLTextureUnitID_GColor)
			{
				const GLuint convertProgramID = (this->_outputFormat == NDSColorFormat_BGR666_Rev) ? OGLRef.programFramebufferRGBA6665OutputID[1] : OGLRef.programFramebufferRGBA8888OutputID[1];
				glUseProgram(convertProgramID);
				glDrawBuffer(OGL_WORKING_ATTACHMENT_ID);
				glReadBuffer(OGL_WORKING_ATTACHMENT_ID);
				this->_lastTextureDrawTarget = OGLTextureUnitID_FinalColor;
			}
			else
			{
				const GLuint convertProgramID = (this->_outputFormat == NDSColorFormat_BGR666_Rev) ? OGLRef.programFramebufferRGBA6665OutputID[0] : OGLRef.programFramebufferRGBA8888OutputID[0];
				glUseProgram(convertProgramID);
				glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
				glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
				this->_lastTextureDrawTarget = OGLTextureUnitID_GColor;
			}
		}
		else
		{
			const GLuint convertProgramID = (this->_outputFormat == NDSColorFormat_BGR666_Rev) ? OGLRef.programFramebufferRGBA6665OutputID[0] : OGLRef.programFramebufferRGBA8888OutputID[0];
			glUseProgram(convertProgramID);
			
			glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FinalColor);
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
			glActiveTexture(GL_TEXTURE0);
		}
		
		glViewport(0, 0, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
		
		glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
		
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
		
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		
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
		
		const GLenum flipTarget = (this->_lastTextureDrawTarget == OGLTextureUnitID_GColor) ? OGL_WORKING_ATTACHMENT_ID : OGL_COLOROUT_ATTACHMENT_ID;
		
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.fboFramebufferFlipID);
		glDrawBuffer(flipTarget);
		
		glBlitFramebufferEXT(0, (GLint)this->_framebufferHeight, (GLint)this->_framebufferWidth, 0, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboFramebufferFlipID);
		glReadBuffer(flipTarget);
	}
	
	if (this->isPBOSupported)
	{
		// Read back the pixels in BGRA format, since legacy OpenGL devices may experience a performance
		// penalty if the readback is in any other format.
		if (this->_mappedFramebuffer != NULL)
		{
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			this->_mappedFramebuffer = NULL;
		}
		
		glReadPixels(0, 0, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, OGLRef.readPixelsBestFormat, OGLRef.readPixelsBestDataType, 0);
	}
	
	this->_pixelReadNeedsFinish = true;
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::BeginRender(const GFX3D_State &renderState, const GFX3D_GeometryList &renderGList)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!BEGINGL())
	{
		return OGLERROR_BEGINGL_FAILED;
	}
	
	this->_clippedPolyCount = renderGList.clippedPolyCount;
	this->_clippedPolyOpaqueCount = renderGList.clippedPolyOpaqueCount;
	this->_clippedPolyList = (CPoly *)renderGList.clippedPolyList;
	this->_rawPolyList = (POLY *)renderGList.rawPolyList;
	
	this->_enableAlphaBlending = (renderState.DISP3DCNT.EnableAlphaBlending) ? true : false;
	
	if (this->_clippedPolyCount > 0)
	{
		if (this->isVBOSupported)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
			
			if (this->isShaderSupported)
			{
				glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
				glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(NDSVertex) * renderGList.rawVertCount, renderGList.rawVtxList);
			}
			else
			{
				// If shaders aren't supported, we need to use the client-side buffers here.
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}
		}
		
		// Generate the clipped polygon list.
		bool renderNeedsToonTable = false;
		
		for (size_t i = 0, vertIndexCount = 0; i < this->_clippedPolyCount; i++)
		{
			const CPoly &cPoly = this->_clippedPolyList[i];
			const POLY &rawPoly = this->_rawPolyList[cPoly.index];
			const size_t polyType = rawPoly.type;
			
			if (this->isShaderSupported)
			{
				for (size_t j = 0; j < polyType; j++)
				{
					const GLushort vertIndex = rawPoly.vertIndexes[j];
					
					// While we're looping through our vertices, add each vertex index to
					// a buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
					// vertices here to convert them to GL_TRIANGLES, which are much easier
					// to work with and won't be deprecated in future OpenGL versions.
					OGLRef.vertIndexBuffer[vertIndexCount++] = vertIndex;
					if (!GFX3D_IsPolyWireframe(rawPoly) && (rawPoly.vtxFormat == GFX3D_QUADS || rawPoly.vtxFormat == GFX3D_QUAD_STRIP))
					{
						if (j == 2)
						{
							OGLRef.vertIndexBuffer[vertIndexCount++] = vertIndex;
						}
						else if (j == 3)
						{
							OGLRef.vertIndexBuffer[vertIndexCount++] = rawPoly.vertIndexes[0];
						}
					}
				}
			}
			else
			{
				const GLfloat thePolyAlpha = (GFX3D_IsPolyWireframe(rawPoly)) ? 1.0f : divide5bitBy31_LUT[rawPoly.attribute.Alpha];
				
				for (size_t j = 0; j < polyType; j++)
				{
					const GLushort vtxIndex = rawPoly.vertIndexes[j];
					const size_t positionIndex = vtxIndex * 4;
					const size_t texCoordIndex = vtxIndex * 2;
					const size_t colorIndex = vtxIndex * 4;
					const NDSVertex &vtx = renderGList.rawVtxList[vtxIndex];
					
					// Since we can't use shaders, we can't perform any data format conversions
					// of the vertex data on the GPU. Therefore, we need to do the conversions
					// on the CPU instead.
					OGLRef.position4fBuffer[positionIndex+0] = (float)vtx.position.x / 4096.0f;
					OGLRef.position4fBuffer[positionIndex+1] = (float)vtx.position.y / 4096.0f;
					OGLRef.position4fBuffer[positionIndex+2] = (float)vtx.position.z / 4096.0f;
					OGLRef.position4fBuffer[positionIndex+3] = (float)vtx.position.w / 4096.0f;
					
					OGLRef.texCoord2fBuffer[texCoordIndex+0] = (float)vtx.texCoord.s / 16.0f;
					OGLRef.texCoord2fBuffer[texCoordIndex+1] = (float)vtx.texCoord.t / 16.0f;
					
					OGLRef.color4fBuffer[colorIndex+0] = divide6bitBy63_LUT[vtx.color.r];
					OGLRef.color4fBuffer[colorIndex+1] = divide6bitBy63_LUT[vtx.color.g];
					OGLRef.color4fBuffer[colorIndex+2] = divide6bitBy63_LUT[vtx.color.b];
					OGLRef.color4fBuffer[colorIndex+3] = thePolyAlpha;
					
					// While we're looping through our vertices, add each vertex index to a
					// buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
					// vertices here to convert them to GL_TRIANGLES, which are much easier
					// to work with and won't be deprecated in future OpenGL versions.
					OGLRef.vertIndexBuffer[vertIndexCount++] = vtxIndex;
					if (!GFX3D_IsPolyWireframe(rawPoly) && (rawPoly.vtxFormat == GFX3D_QUADS || rawPoly.vtxFormat == GFX3D_QUAD_STRIP))
					{
						if (j == 2)
						{
							OGLRef.vertIndexBuffer[vertIndexCount++] = vtxIndex;
						}
						else if (j == 3)
						{
							OGLRef.vertIndexBuffer[vertIndexCount++] = rawPoly.vertIndexes[0];
						}
					}
				}
			}
			
			renderNeedsToonTable = (renderNeedsToonTable || (rawPoly.attribute.Mode == POLYGON_MODE_TOONHIGHLIGHT)) && this->isShaderSupported;
			
			// Get the texture that is to be attached to this polygon.
			this->_textureList[i] = this->GetLoadedTextureFromPolygon(rawPoly, this->_enableTextureSampling);
		}
		
		if (this->isVBOSupported)
		{
			// Replace the entire index buffer as a hint to the driver that we can orphan the index buffer and
			// avoid a synchronization cost.
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(OGLRef.vertIndexBuffer), OGLRef.vertIndexBuffer);
		}
		
		// Set up rendering states that will remain constant for the entire frame.
		this->_pendingRenderStates.enableAntialiasing = (renderState.DISP3DCNT.EnableAntialiasing) ? GL_TRUE : GL_FALSE;
		this->_pendingRenderStates.enableFogAlphaOnly = (renderState.DISP3DCNT.FogOnlyAlpha) ? GL_TRUE : GL_FALSE;
		this->_pendingRenderStates.clearPolyID = this->_clearAttributes.opaquePolyID;
		this->_pendingRenderStates.clearDepth = (GLfloat)this->_clearAttributes.depth / (GLfloat)0x00FFFFFF;
		this->_pendingRenderStates.alphaTestRef = divide5bitBy31_LUT[renderState.alphaTestRef];
		
		if (this->_enableFog && this->_deviceInfo.isFogSupported)
		{
			this->_fogProgramKey.key = 0;
			this->_fogProgramKey.offset = renderState.fogOffset & 0x7FFF;
			this->_fogProgramKey.shift = renderState.fogShift;
			
			this->_pendingRenderStates.fogColor.r = divide5bitBy31_LUT[(renderState.fogColor      ) & 0x0000001F];
			this->_pendingRenderStates.fogColor.g = divide5bitBy31_LUT[(renderState.fogColor >>  5) & 0x0000001F];
			this->_pendingRenderStates.fogColor.b = divide5bitBy31_LUT[(renderState.fogColor >> 10) & 0x0000001F];
			this->_pendingRenderStates.fogColor.a = divide5bitBy31_LUT[(renderState.fogColor >> 16) & 0x0000001F];
			this->_pendingRenderStates.fogOffset = (GLfloat)(renderState.fogOffset & 0x7FFF) / 32767.0f;
			this->_pendingRenderStates.fogStep = (GLfloat)(0x0400 >> renderState.fogShift) / 32767.0f;
			
			u8 fogDensityTable[32];
			for (size_t i = 0; i < 32; i++)
			{
				fogDensityTable[i] = (renderState.fogDensityTable[i] == 127) ? 255 : renderState.fogDensityTable[i] << 1;
			}
			
			glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_LookupTable);
			glBindTexture(GL_TEXTURE_1D, OGLRef.texFogDensityTableID);
			glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 32, GL_RED, GL_UNSIGNED_BYTE, fogDensityTable);
		}
		
		if (this->_enableEdgeMark && this->_deviceInfo.isEdgeMarkSupported)
		{
			const u8 alpha8 = (renderState.DISP3DCNT.EnableAntialiasing) ? 0x80 : 0xFF;
			Color4u8 edgeColor32[8];
			
			for (size_t i = 0; i < 8; i++)
			{
				edgeColor32[i].value = COLOR555TO8888(renderState.edgeMarkColorTable[i] & 0x7FFF, alpha8);
			}
			
			glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_LookupTable);
			glBindTexture(GL_TEXTURE_1D, OGLRef.texEdgeColorTableID);
			glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 8, GL_RGBA, OGL_TEXTURE_SRC_EDGE_COLOR, edgeColor32);
		}
		
		if (this->isShaderSupported)
		{
			this->_geometryProgramFlags.value = 0;
			this->_geometryProgramFlags.EnableWDepth = renderState.SWAP_BUFFERS.DepthMode;
			this->_geometryProgramFlags.EnableAlphaTest = renderState.DISP3DCNT.EnableAlphaTest;
			this->_geometryProgramFlags.EnableTextureSampling = (this->_enableTextureSampling) ? 1 : 0;
			this->_geometryProgramFlags.ToonShadingMode = renderState.DISP3DCNT.PolygonShading;
			this->_geometryProgramFlags.EnableFog = (this->_enableFog && this->_deviceInfo.isFogSupported) ? 1 : 0;
			this->_geometryProgramFlags.EnableEdgeMark = (this->_enableEdgeMark && this->_deviceInfo.isEdgeMarkSupported) ? 1 : 0;
			this->_geometryProgramFlags.OpaqueDrawMode = (this->_isDepthLEqualPolygonFacingSupported) ? 1 : 0;
			
			this->_SetupGeometryShaders(this->_geometryProgramFlags);
			
			if (renderNeedsToonTable)
			{
				glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_LookupTable);
				glBindTexture(GL_TEXTURE_1D, OGLRef.texToonTableID);
				
				if (OGL_TEXTURE_SRC_TOON_TABLE == GL_UNSIGNED_BYTE)
				{
					ColorspaceConvertBuffer5551To8888<false, false, BESwapDst>(renderState.toonTable16, OGLRef.toonTable32, 32);
					glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 32, GL_RGBA, OGL_TEXTURE_SRC_TOON_TABLE, OGLRef.toonTable32);
				}
				else
				{
					glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 32, GL_RGBA, OGL_TEXTURE_SRC_TOON_TABLE, renderState.toonTable16);
				}
			}
		}
#if !defined(GL_ES_VERSION_3_0)
		else
		{
			if (renderState.DISP3DCNT.EnableAlphaTest && (renderState.alphaTestRef > 0))
			{
				glAlphaFunc(GL_GEQUAL, divide5bitBy31_LUT[renderState.alphaTestRef]);
			}
			else
			{
				glAlphaFunc(GL_GREATER, 0);
			}
			
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			
			if (this->isFBOSupported)
			{
				glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
			}
		}
#endif
	}
	else
	{
		if (this->isShaderSupported && this->isFBOSupported)
		{
			// Even with no polygons to draw, we always need to set these 3 flags so that
			// glDrawBuffers() can reference the correct set of FBO attachments using
			// OGLGeometryFlags.DrawBuffersMode.
			this->_geometryProgramFlags.EnableFog = (this->_enableFog) ? 1 : 0;
			this->_geometryProgramFlags.EnableEdgeMark = (this->_enableEdgeMark) ? 1 : 0;
			this->_geometryProgramFlags.OpaqueDrawMode = 1;
		}
	}
	
	glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	
	this->_needsZeroDstAlphaPass = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::RenderGeometry()
{
	if (this->_clippedPolyCount > 0)
	{
		glDisable(GL_CULL_FACE); // Polygons should already be culled before we get here.
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_STENCIL_TEST);
		
		if (this->_enableAlphaBlending)
		{
			glEnable(GL_BLEND);
		}
		else
		{
			glDisable(GL_BLEND);
		}
		
		glActiveTexture(GL_TEXTURE0);
		
		this->EnableVertexAttributes();
		
		size_t indexOffset = 0;
		
		const CPoly &firstCPoly = this->_clippedPolyList[0];
		const POLY *rawPolyList = this->_rawPolyList;
		const POLY &firstPoly = rawPolyList[firstCPoly.index];
		POLYGON_ATTR lastPolyAttr = firstPoly.attribute;
		
		if (this->_clippedPolyOpaqueCount > 0)
		{
			this->SetupPolygon(firstPoly, false, true, firstCPoly.isPolyBackFacing);
			this->DrawPolygonsForIndexRange<OGLPolyDrawMode_DrawOpaquePolys>(rawPolyList, this->_clippedPolyList, this->_clippedPolyCount, 0, this->_clippedPolyOpaqueCount - 1, indexOffset, lastPolyAttr);
		}
		
		if (this->_clippedPolyOpaqueCount < this->_clippedPolyCount)
		{
			this->_geometryProgramFlags.OpaqueDrawMode = 0;
			
			if (this->_needsZeroDstAlphaPass && this->_emulateSpecialZeroAlphaBlending)
			{
				if (this->_clippedPolyOpaqueCount == 0)
				{
					this->SetupPolygon(firstPoly, true, false, firstCPoly.isPolyBackFacing);
				}
				
				this->ZeroDstAlphaPass(rawPolyList, this->_clippedPolyList, this->_clippedPolyCount, this->_clippedPolyOpaqueCount, this->_enableAlphaBlending, indexOffset, lastPolyAttr);
				
				if (this->_clippedPolyOpaqueCount > 0)
				{
					const CPoly &lastOpaqueCPoly = this->_clippedPolyList[this->_clippedPolyOpaqueCount - 1];
					const POLY &lastOpaquePoly = rawPolyList[lastOpaqueCPoly.index];
					lastPolyAttr = lastOpaquePoly.attribute;
					this->SetupPolygon(lastOpaquePoly, false, true, lastOpaqueCPoly.isPolyBackFacing);
				}
			}
			else
			{
				// If we're not doing the zero-dst-alpha pass, then we need to make sure to
				// clear the stencil bit that we will use to mark transparent fragments.
				glStencilMask(0x40);
				glClearStencil(0);
				glClear(GL_STENCIL_BUFFER_BIT);
				glStencilMask(0xFF);
				
				this->_SetupGeometryShaders(this->_geometryProgramFlags);
			}
			
			if (this->_clippedPolyOpaqueCount == 0)
			{
				this->SetupPolygon(firstPoly, true, true, firstCPoly.isPolyBackFacing);
			}
			else
			{
				this->_ResolveWorkingBackFacing();
			}
			
			this->DrawPolygonsForIndexRange<OGLPolyDrawMode_DrawTranslucentPolys>(rawPolyList, this->_clippedPolyList, this->_clippedPolyCount, this->_clippedPolyOpaqueCount, this->_clippedPolyCount - 1, indexOffset, lastPolyAttr);
		}
		
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);
		this->DisableVertexAttributes();
	}
	
	this->_ResolveGeometry();
	
	this->_lastTextureDrawTarget = OGLTextureUnitID_GColor;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::PostprocessFramebuffer()
{
	if (this->_clippedPolyCount < 1)
	{
		return OGLERROR_NOERR;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (this->_enableEdgeMark && this->_deviceInfo.isEdgeMarkSupported) ||
		 (this->_enableFog && this->_deviceInfo.isFogSupported) )
	{
		// Set up the postprocessing states
		glViewport(0, 0, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
		glDisable(GL_DEPTH_TEST);
		
		glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
		
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
	}
	else
	{
		return OGLERROR_NOERR;
	}
	
	if (this->_enableEdgeMark && this->_deviceInfo.isEdgeMarkSupported)
	{
		if (this->_needsZeroDstAlphaPass && this->_emulateSpecialZeroAlphaBlending)
		{
			// Pass 1: Determine the pixels with zero alpha
			glDrawBuffer(GL_NONE);
			glDisable(GL_BLEND);
			glEnable(GL_STENCIL_TEST);
			glStencilFunc(GL_ALWAYS, 0x40, 0x40);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
			glStencilMask(0x40);
			
			glUseProgram(OGLRef.programGeometryZeroDstAlphaID);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			
			// Pass 2: Unblended edge mark colors to zero-alpha pixels
			glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_LookupTable);
			glBindTexture(GL_TEXTURE_1D, OGLRef.texEdgeColorTableID);
			glActiveTexture(GL_TEXTURE0);
			
			glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
			glUseProgram(OGLRef.programEdgeMarkID);
			glUniform1i(OGLRef.uniformStateClearPolyID, this->_pendingRenderStates.clearPolyID);
			glUniform1f(OGLRef.uniformStateClearDepth, this->_pendingRenderStates.clearDepth);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
			glStencilFunc(GL_NOTEQUAL, 0x40, 0x40);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			
			// Pass 3: Blended edge mark
			glEnable(GL_BLEND);
			glDisable(GL_STENCIL_TEST);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
		else
		{
			glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_LookupTable);
			glBindTexture(GL_TEXTURE_1D, OGLRef.texEdgeColorTableID);
			glActiveTexture(GL_TEXTURE0);
			
			glUseProgram(OGLRef.programEdgeMarkID);
			glUniform1i(OGLRef.uniformStateClearPolyID, this->_pendingRenderStates.clearPolyID);
			glUniform1f(OGLRef.uniformStateClearDepth, this->_pendingRenderStates.clearDepth);
			glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
			glEnable(GL_BLEND);
			glDisable(GL_STENCIL_TEST);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
	}
	
	if (this->_enableFog && this->_deviceInfo.isFogSupported)
	{
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_LookupTable);
		glBindTexture(GL_TEXTURE_1D, OGLRef.texFogDensityTableID);
		glActiveTexture(GL_TEXTURE0);
		
		std::map<u32, OGLFogShaderID>::iterator it = this->_fogProgramMap.find(this->_fogProgramKey.key);
		if (it == this->_fogProgramMap.end())
		{
			Render3DError error = this->CreateFogProgram(this->_fogProgramKey, FogVtxShader_100, FogFragShader_100);
			if (error != OGLERROR_NOERR)
			{
				return error;
			}
		}
		
		OGLFogShaderID shaderID = this->_fogProgramMap[this->_fogProgramKey.key];
		
		glDrawBuffer(OGL_WORKING_ATTACHMENT_ID);
		glUseProgram(shaderID.program);
		glUniform1i(OGLRef.uniformStateEnableFogAlphaOnly, this->_pendingRenderStates.enableFogAlphaOnly);
		glUniform4fv(OGLRef.uniformStateFogColor, 1, (const GLfloat *)&this->_pendingRenderStates.fogColor);
		
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		
		this->_lastTextureDrawTarget = OGLTextureUnitID_FinalColor;
	}
	
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

Render3DError OpenGLRenderer_1_2::EndRender()
{
	//needs to happen before endgl because it could free some textureids for expired cache items
	texCache.Evict();
	
	this->ReadBackPixels();
	
	ENDGL();
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID)
{
	if (!this->isFBOSupported || !this->_isFBOBlitSupported)
	{
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	this->UploadClearImage(colorBuffer, depthBuffer, fogBuffer, opaquePolyID);
	
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboClearImageID);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
	
	// It might seem wasteful to be doing a separate glClear(GL_STENCIL_BUFFER_BIT) instead
	// of simply blitting the stencil buffer with everything else.
	//
	// We do this because glBlitFramebufferEXT() for GL_STENCIL_BUFFER_BIT has been tested
	// to be unsupported on ATI/AMD GPUs running in compatibility mode. So we do the separate
	// glClear() for GL_STENCIL_BUFFER_BIT to keep these GPUs working.
	glClearStencil(opaquePolyID);
	glClear(GL_STENCIL_BUFFER_BIT);
	
	if (this->isShaderSupported)
	{
		if (this->_emulateDepthLEqualPolygonFacing && this->_isDepthLEqualPolygonFacingSupported)
		{
			glDrawBuffer(OGL_WORKING_ATTACHMENT_ID);
			glClearColor(0.0, 0.0, 0.0, 0.0);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		
		if (this->_enableEdgeMark && this->_deviceInfo.isEdgeMarkSupported)
		{
			glDrawBuffer(OGL_POLYID_ATTACHMENT_ID);
			glClearColor((GLfloat)opaquePolyID/63.0f, 0.0, 0.0, 1.0);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		
		if (this->_enableFog && this->_deviceInfo.isFogSupported)
		{
			glReadBuffer(OGL_FOGATTRIBUTES_ATTACHMENT_ID);
			glDrawBuffer(OGL_FOGATTRIBUTES_ATTACHMENT_ID);
			glBlitFramebufferEXT(0, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GPU_FRAMEBUFFER_NATIVE_WIDTH, 0, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		
		// Blit the color buffer. Do this last so that color attachment 0 is set to the read FBO.
		glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
		glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
		glBlitFramebufferEXT(0, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GPU_FRAMEBUFFER_NATIVE_WIDTH, 0, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
		glDrawBuffers(4, GeometryDrawBuffersEnum[this->_geometryProgramFlags.DrawBuffersMode]);
	}
	else
	{
		glBlitFramebufferEXT(0, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GPU_FRAMEBUFFER_NATIVE_WIDTH, 0, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
		glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
		glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	}
	
	if (this->isMultisampledFBOSupported)
	{
		OGLRef.selectedRenderingFBO = (this->_enableMultisampledRendering) ? OGLRef.fboMSIntermediateRenderID : OGLRef.fboRenderID;
		if (OGLRef.selectedRenderingFBO == OGLRef.fboMSIntermediateRenderID)
		{
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
			glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
			
			// See above comment for why we need to get clear the stencil buffer separately.
			glClearStencil(opaquePolyID);
			glClear(GL_STENCIL_BUFFER_BIT);
			
			if (this->isShaderSupported)
			{
				if (this->_emulateDepthLEqualPolygonFacing && this->_isDepthLEqualPolygonFacingSupported)
				{
					glDrawBuffer(OGL_WORKING_ATTACHMENT_ID);
					glClearColor(0.0, 0.0, 0.0, 0.0);
					glClear(GL_COLOR_BUFFER_BIT);
				}
				
				if (this->_enableEdgeMark && this->_deviceInfo.isEdgeMarkSupported)
				{
					glDrawBuffer(OGL_POLYID_ATTACHMENT_ID);
					glClearColor((GLfloat)opaquePolyID/63.0f, 0.0, 0.0, 1.0);
					glClear(GL_COLOR_BUFFER_BIT);
				}
				
				if (this->_enableFog && this->_deviceInfo.isFogSupported)
				{
					glReadBuffer(OGL_FOGATTRIBUTES_ATTACHMENT_ID);
					glDrawBuffer(OGL_FOGATTRIBUTES_ATTACHMENT_ID);
					glBlitFramebufferEXT(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
				}
				
				// Blit the color and depth buffers. Do this last so that color attachment 0 is set to the read FBO.
				glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
				glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
				glBlitFramebufferEXT(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
				
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
				glDrawBuffers(4, GeometryDrawBuffersEnum[this->_geometryProgramFlags.DrawBuffersMode]);
			}
			else
			{
				// Blit the color and depth buffers.
				glBlitFramebufferEXT(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
				
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
				glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
				glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
			}
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ClearUsingValues(const Color4u8 &clearColor6665, const FragmentAttributes &clearAttributes)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isFBOSupported)
	{
		OGLRef.selectedRenderingFBO = (this->_enableMultisampledRendering) ? OGLRef.fboMSIntermediateRenderID : OGLRef.fboRenderID;
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
	}
	
	if (this->isShaderSupported && this->isFBOSupported)
	{
		glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
		glClearColor(divide6bitBy63_LUT[clearColor6665.r], divide6bitBy63_LUT[clearColor6665.g], divide6bitBy63_LUT[clearColor6665.b], divide5bitBy31_LUT[clearColor6665.a]);
		glClearDepth((GLclampd)clearAttributes.depth / (GLclampd)0x00FFFFFF);
		glClearStencil(clearAttributes.opaquePolyID);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		
		if (this->_emulateDepthLEqualPolygonFacing && this->_isDepthLEqualPolygonFacingSupported)
		{
			glDrawBuffer(OGL_WORKING_ATTACHMENT_ID);
			glClearColor(0.0, 0.0, 0.0, 0.0);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		
		if (this->_enableEdgeMark && this->_deviceInfo.isEdgeMarkSupported)
		{
			glDrawBuffer(OGL_POLYID_ATTACHMENT_ID);
			glClearColor((GLfloat)clearAttributes.opaquePolyID/63.0f, 0.0, 0.0, 1.0);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		
		if (this->_enableFog && this->_deviceInfo.isFogSupported)
		{
			glDrawBuffer(OGL_FOGATTRIBUTES_ATTACHMENT_ID);
			glClearColor(clearAttributes.isFogged, 0.0, 0.0, 1.0);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		
		glDrawBuffers(4, GeometryDrawBuffersEnum[this->_geometryProgramFlags.DrawBuffersMode]);
		this->_needsZeroDstAlphaPass = (clearColor6665.a == 0);
	}
	else
	{
		if (this->isFBOSupported)
		{
			glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
			glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
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

Render3DError OpenGLRenderer_1_2::SetupPolygon(const POLY &thePoly, bool treatAsTranslucent, bool willChangeStencilBuffer, bool isBackFacing)
{
	// Set up depth test mode
	glDepthFunc((thePoly.attribute.DepthEqualTest_Enable) ? GL_EQUAL : GL_LESS);
	
	if (willChangeStencilBuffer)
	{
		// Handle drawing states for the polygon
		if (thePoly.attribute.Mode == POLYGON_MODE_SHADOW)
		{
			if (this->_emulateShadowPolygon)
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
		glUniform1i(OGLRef.uniformPolyMode[this->_geometryProgramFlags.value], thePoly.attribute.Mode);
		glUniform1i(OGLRef.uniformPolyEnableFog[this->_geometryProgramFlags.value], (thePoly.attribute.Fog_Enable) ? GL_TRUE : GL_FALSE);
		glUniform1f(OGLRef.uniformPolyAlpha[this->_geometryProgramFlags.value], (GFX3D_IsPolyWireframe(thePoly)) ? 1.0f : divide5bitBy31_LUT[thePoly.attribute.Alpha]);
		glUniform1i(OGLRef.uniformPolyID[this->_geometryProgramFlags.value], thePoly.attribute.PolygonID);
		glUniform1i(OGLRef.uniformPolyIsWireframe[this->_geometryProgramFlags.value], (GFX3D_IsPolyWireframe(thePoly)) ? GL_TRUE : GL_FALSE);
		glUniform1i(OGLRef.uniformPolySetNewDepthForTranslucent[this->_geometryProgramFlags.value], (thePoly.attribute.TranslucentDepthWrite_Enable) ? GL_TRUE : GL_FALSE);
		glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], 0.0f);
		glUniform1i(OGLRef.uniformPolyIsBackFacing[this->_geometryProgramFlags.value], (isBackFacing) ? GL_TRUE : GL_FALSE);
	}
#if !defined(GL_ES_VERSION_3_0)
	else
	{
		// Set the texture blending mode
		static const GLint oglTexBlendMode[4] = {GL_MODULATE, GL_DECAL, GL_MODULATE, GL_MODULATE};
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, oglTexBlendMode[thePoly.attribute.Mode]);
	}
#endif
	
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
			glUniform1i(OGLRef.uniformPolyEnableTexture[this->_geometryProgramFlags.value], GL_FALSE);
			glUniform1i(OGLRef.uniformTexSingleBitAlpha[this->_geometryProgramFlags.value], GL_FALSE);
			glUniform2f(OGLRef.uniformPolyTexScale[this->_geometryProgramFlags.value], theTexture->GetInvWidth(), theTexture->GetInvHeight());
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
		glUniform1i(OGLRef.uniformPolyEnableTexture[this->_geometryProgramFlags.value], GL_TRUE);
		glUniform1i(OGLRef.uniformTexSingleBitAlpha[this->_geometryProgramFlags.value], (packFormat != TEXMODE_A3I5 && packFormat != TEXMODE_A5I3) ? GL_TRUE : GL_FALSE);
		glUniform2f(OGLRef.uniformPolyTexScale[this->_geometryProgramFlags.value], theTexture->GetInvWidth(), theTexture->GetInvHeight());
	}
#if !defined(GL_ES_VERSION_3_0)
	else
	{
		glEnable(GL_TEXTURE_2D);
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glScalef(theTexture->GetInvWidth(), theTexture->GetInvHeight(), 1.0f);
	}
#endif
	
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

Render3DError OpenGLRenderer_1_2::SetupViewport(const GFX3D_Viewport viewport)
{
	const GLfloat wScalar = this->_framebufferWidth  / (GLfloat)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const GLfloat hScalar = this->_framebufferHeight / (GLfloat)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	glViewport(viewport.x * wScalar,
	           viewport.y * hScalar,
	           viewport.width * wScalar,
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
	// 4th pass (emulator driven): Use stencil buffer bit 7 (0x80) for the shadow volume
	// mask and write out the polygon ID and translucent fragment flag only to those fragments
	// within the mask. Color and depth writes are disabled for this pass.
	//
	// 5th pass (emulator driven): Use stencil buffer bit 7 (0x80) for the shadow volume
	// mask and draw the shadow polygon fragments only within the mask. Color writes are always
	// enabled and depth writes are enabled if the shadow polygon is opaque or if transparent
	// polygon depth writes are enabled.
	
	// 1st pass: Create the shadow volume.
	if (opaquePolyID == 0)
	{
		if (performDepthEqualTest && this->_emulateNDSDepthCalculation && this->isShaderSupported)
		{
			// Use the stencil buffer to determine which fragments fail the depth test using the lower-side tolerance.
			glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], (float)DEPTH_EQUALS_TEST_TOLERANCE / 16777215.0f);
			glDepthFunc(GL_LEQUAL);
			glStencilFunc(GL_ALWAYS, 0x80, 0x80);
			glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);
			glStencilMask(0x80);
			glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
			
			// Use the stencil buffer to determine which fragments fail the depth test using the higher-side tolerance.
			glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], (float)-DEPTH_EQUALS_TEST_TOLERANCE / 16777215.0f);
			glDepthFunc(GL_GEQUAL);
			glStencilFunc(GL_NOTEQUAL, 0x80, 0x80);
			glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);
			glStencilMask(0x80);
			glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
			
			glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], 0.0f);
		}
		else
		{
			glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		}
		
		return OGLERROR_NOERR;
	}
	
	// 2nd pass: Do the polygon ID check.
	if (performDepthEqualTest && this->_emulateNDSDepthCalculation && this->isShaderSupported)
	{
		// Use the stencil buffer to determine which fragments pass the lower-side tolerance.
		glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], (float)DEPTH_EQUALS_TEST_TOLERANCE / 16777215.0f);
		glDepthFunc(GL_LEQUAL);
		glStencilFunc(GL_EQUAL, 0x80, 0x80);
		glStencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
		glStencilMask(0x80);
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		
		// Use the stencil buffer to determine which fragments pass the higher-side tolerance.
		glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], (float)-DEPTH_EQUALS_TEST_TOLERANCE / 16777215.0f);
		glDepthFunc(GL_GEQUAL);
		glStencilFunc(GL_EQUAL, 0x80, 0x80);
		glStencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
		glStencilMask(0x80);
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		
		// Finally, do the polygon ID check.
		glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], 0.0f);
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
		glUniform1i(OGLRef.uniformPolyDrawShadow[this->_geometryProgramFlags.value], GL_TRUE);
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		glUniform1i(OGLRef.uniformPolyDrawShadow[this->_geometryProgramFlags.value], GL_FALSE);
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
	
#if !defined(GL_ES_VERSION_3_0)
	if (!this->isShaderSupported)
	{
		glEnable(GL_NORMALIZE);
		glEnable(GL_TEXTURE_1D);
		glEnable(GL_TEXTURE_2D);
		glAlphaFunc(GL_GREATER, 0);
		glEnable(GL_ALPHA_TEST);
		glEnable(GL_BLEND);
	}
#endif
	
	ENDGL();
	
	this->_pixelReadNeedsFinish = false;
	
	if (OGLRef.position4fBuffer != NULL)
	{
		memset(OGLRef.position4fBuffer, 0, VERTLIST_SIZE * 4 * sizeof(GLfloat));
	}
	
	if (OGLRef.texCoord2fBuffer != NULL)
	{
		memset(OGLRef.texCoord2fBuffer, 0, VERTLIST_SIZE * 2 * sizeof(GLfloat));
	}
	
	if (OGLRef.color4fBuffer != NULL)
	{
		memset(OGLRef.color4fBuffer, 0, VERTLIST_SIZE * 4 * sizeof(GLfloat));
	}
	
	this->_currentPolyIndex = 0;
	
	memset(&this->_pendingRenderStates, 0, sizeof(this->_pendingRenderStates));
	
	texCache.Reset();
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::RenderPowerOff()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->_isPoweredOn)
	{
		return OGLERROR_NOERR;
	}
	
	this->_isPoweredOn = false;
	memset(GPU->GetEngineMain()->Get3DFramebufferMain(), 0, this->_framebufferColorSizeBytes);
	memset(GPU->GetEngineMain()->Get3DFramebuffer16(), 0, this->_framebufferPixCount * sizeof(u16));
	
	if(!BEGINGL())
	{
		return OGLERROR_BEGINGL_FAILED;
	}
	
	if (this->isFBOSupported)
	{
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboRenderID);
		glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
		glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	}
	
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	
	if (this->isPBOSupported)
	{
		if (this->_mappedFramebuffer != NULL)
		{
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			this->_mappedFramebuffer = NULL;
		}
		
		glReadPixels(0, 0, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, OGLRef.readPixelsBestFormat, OGLRef.readPixelsBestDataType, 0);
	}
	
	ENDGL();
	
	this->_pixelReadNeedsFinish = true;
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::RenderFinish()
{
	OGLRenderRef &OGLRef = *this->ref;
	
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
			this->_mappedFramebuffer = (Color4u8 *__restrict)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		}
		else
		{
			glReadPixels(0, 0, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, OGLRef.readPixelsBestFormat, OGLRef.readPixelsBestDataType, this->_framebufferColor);
		}
		
		ENDGL();
	}
	
	this->_renderNeedsFlushMain = true;
	this->_renderNeedsFlush16 = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::RenderFlush(bool willFlushBuffer32, bool willFlushBuffer16)
{
	if (!this->_isPoweredOn)
	{
		return RENDER3DERROR_NOERR;
	}
	
	Color4u8 *framebufferMain = (willFlushBuffer32) ? GPU->GetEngineMain()->Get3DFramebufferMain() : NULL;
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
	Render3DError error = OGLERROR_NOERR;
	
	if (w < GPU_FRAMEBUFFER_NATIVE_WIDTH || h < GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		return error;
	}
	
	if (!BEGINGL())
	{
		error = OGLERROR_BEGINGL_FAILED;
		return error;
	}
	
	glFinish();
	
	const size_t newFramebufferColorSizeBytes = w * h * sizeof(Color4u8);
	
	if (this->isPBOSupported)
	{
		if (this->_mappedFramebuffer != NULL)
		{
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			glFinish();
		}
		
		glBufferData(GL_PIXEL_PACK_BUFFER, newFramebufferColorSizeBytes, NULL, GL_STREAM_READ);
		
		if (this->_mappedFramebuffer != NULL)
		{
			this->_mappedFramebuffer = (Color4u8 *__restrict)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
			glFinish();
		}
	}
	
	if (this->isShaderSupported || this->isFBOSupported)
	{
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FinalColor);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	}
	
	if (this->isFBOSupported)
	{
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_DepthStencil);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, (GLsizei)w, (GLsizei)h, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
		
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GColor);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GPolyID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FogAttr);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	}
	
	glActiveTexture(GL_TEXTURE0);
	
	this->_framebufferWidth = w;
	this->_framebufferHeight = h;
	this->_framebufferPixCount = w * h;
	this->_framebufferColorSizeBytes = newFramebufferColorSizeBytes;
	
	// Call ResizeMultisampledFBOs() after _framebufferWidth and _framebufferHeight are set
	// since this method depends on them.
	GLsizei sampleSize = this->GetLimitedMultisampleSize();
	this->ResizeMultisampledFBOs(sampleSize);
	
	if (this->isPBOSupported)
	{
		this->_framebufferColor = NULL;
	}
	else
	{
		Color4u8 *oldFramebufferColor = this->_framebufferColor;
		Color4u8 *newFramebufferColor = (Color4u8 *)malloc_alignedCacheLine(newFramebufferColorSizeBytes);
		this->_framebufferColor = newFramebufferColor;
		free_aligned(oldFramebufferColor);
	}
	
	if (this->isShaderSupported)
	{
		// Recreate shaders that use the framebuffer size.
		glUseProgram(0);
		this->DestroyEdgeMarkProgram();
		this->DestroyFramebufferOutput6665Programs();
		this->DestroyFramebufferOutput8888Programs();
		this->DestroyGeometryPrograms();
		
		this->CreateGeometryPrograms();
		
		if (this->isVBOSupported && this->isFBOSupported)
		{
			this->CreateEdgeMarkProgram(EdgeMarkVtxShader_100, EdgeMarkFragShader_100);
		}
		
		this->CreateFramebufferOutput6665Program(0, FramebufferOutputVtxShader_100, FramebufferOutputRGBA6665FragShader_100);
		this->CreateFramebufferOutput6665Program(1, FramebufferOutputVtxShader_100, FramebufferOutputRGBA6665FragShader_100);
		this->CreateFramebufferOutput8888Program(0, FramebufferOutputVtxShader_100, FramebufferOutputRGBA8888FragShader_100);
		this->CreateFramebufferOutput8888Program(1, FramebufferOutputVtxShader_100, FramebufferOutputRGBA8888FragShader_100);
	}
	
	if (oglrender_framebufferDidResizeCallback != NULL)
	{
		bool clientResizeSuccess = oglrender_framebufferDidResizeCallback(this->isFBOSupported, w, h);
		if (!clientResizeSuccess)
		{
			error = OGLERROR_CLIENT_RESIZE_ERROR;
		}
	}
	
	glFinish();
	ENDGL();
	
	return error;
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
	OGLRef.position4fBuffer = NULL;
	OGLRef.texCoord2fBuffer = NULL;
	OGLRef.color4fBuffer = NULL;
	
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
		glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_INT, GL_FALSE, sizeof(NDSVertex), (const GLvoid *)offsetof(NDSVertex, position));
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_INT, GL_FALSE, sizeof(NDSVertex), (const GLvoid *)offsetof(NDSVertex, texCoord));
		glVertexAttribPointer(OGLVertexAttributeID_Color, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(NDSVertex), (const GLvoid *)offsetof(NDSVertex, color));
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

Render3DError OpenGLRenderer_2_0::BeginRender(const GFX3D_State &renderState, const GFX3D_GeometryList &renderGList)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!BEGINGL())
	{
		return OGLERROR_BEGINGL_FAILED;
	}
	
	this->_clippedPolyCount = renderGList.clippedPolyCount;
	this->_clippedPolyOpaqueCount = renderGList.clippedPolyOpaqueCount;
	this->_clippedPolyList = (CPoly *)renderGList.clippedPolyList;
	this->_rawPolyList = (POLY *)renderGList.rawPolyList;
	
	this->_enableAlphaBlending = (renderState.DISP3DCNT.EnableAlphaBlending) ? true : false;
	
	if (this->_clippedPolyCount > 0)
	{
		glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
		
		// Only copy as much vertex data as we need to, since this can be a potentially large upload size.
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(NDSVertex) * renderGList.rawVertCount, renderGList.rawVtxList);
		
		// Generate the clipped polygon list.
		bool renderNeedsToonTable = false;
		
		for (size_t i = 0, vertIndexCount = 0; i < this->_clippedPolyCount; i++)
		{
			const CPoly &cPoly = this->_clippedPolyList[i];
			const POLY &rawPoly = this->_rawPolyList[cPoly.index];
			const size_t polyType = rawPoly.type;
			
			for (size_t j = 0; j < polyType; j++)
			{
				const GLushort vertIndex = rawPoly.vertIndexes[j];
				
				// While we're looping through our vertices, add each vertex index to
				// a buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
				// vertices here to convert them to GL_TRIANGLES, which are much easier
				// to work with and won't be deprecated in future OpenGL versions.
				OGLRef.vertIndexBuffer[vertIndexCount++] = vertIndex;
				if (!GFX3D_IsPolyWireframe(rawPoly) && (rawPoly.vtxFormat == GFX3D_QUADS || rawPoly.vtxFormat == GFX3D_QUAD_STRIP))
				{
					if (j == 2)
					{
						OGLRef.vertIndexBuffer[vertIndexCount++] = vertIndex;
					}
					else if (j == 3)
					{
						OGLRef.vertIndexBuffer[vertIndexCount++] = rawPoly.vertIndexes[0];
					}
				}
			}
			
			renderNeedsToonTable = renderNeedsToonTable || (rawPoly.attribute.Mode == POLYGON_MODE_TOONHIGHLIGHT);
			
			// Get the texture that is to be attached to this polygon.
			this->_textureList[i] = this->GetLoadedTextureFromPolygon(rawPoly, this->_enableTextureSampling);
		}
		
		// Replace the entire index buffer as a hint to the driver that we can orphan the index buffer and
		// avoid a synchronization cost.
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(OGLRef.vertIndexBuffer), OGLRef.vertIndexBuffer);
		
		// Set up rendering states that will remain constant for the entire frame.
		this->_pendingRenderStates.enableAntialiasing = (renderState.DISP3DCNT.EnableAntialiasing) ? GL_TRUE : GL_FALSE;
		this->_pendingRenderStates.enableFogAlphaOnly = (renderState.DISP3DCNT.FogOnlyAlpha) ? GL_TRUE : GL_FALSE;
		this->_pendingRenderStates.clearPolyID = this->_clearAttributes.opaquePolyID;
		this->_pendingRenderStates.clearDepth = (GLfloat)this->_clearAttributes.depth / (GLfloat)0x00FFFFFF;
		this->_pendingRenderStates.alphaTestRef = divide5bitBy31_LUT[renderState.alphaTestRef];
		
		if (this->_enableFog && this->_deviceInfo.isFogSupported)
		{
			this->_fogProgramKey.key = 0;
			this->_fogProgramKey.offset = renderState.fogOffset & 0x7FFF;
			this->_fogProgramKey.shift = renderState.fogShift;
			
			this->_pendingRenderStates.fogColor.r = divide5bitBy31_LUT[(renderState.fogColor      ) & 0x0000001F];
			this->_pendingRenderStates.fogColor.g = divide5bitBy31_LUT[(renderState.fogColor >>  5) & 0x0000001F];
			this->_pendingRenderStates.fogColor.b = divide5bitBy31_LUT[(renderState.fogColor >> 10) & 0x0000001F];
			this->_pendingRenderStates.fogColor.a = divide5bitBy31_LUT[(renderState.fogColor >> 16) & 0x0000001F];
			this->_pendingRenderStates.fogOffset = (GLfloat)(renderState.fogOffset & 0x7FFF) / 32767.0f;
			this->_pendingRenderStates.fogStep = (GLfloat)(0x0400 >> renderState.fogShift) / 32767.0f;
			
			u8 fogDensityTable[32];
			for (size_t i = 0; i < 32; i++)
			{
				fogDensityTable[i] = (renderState.fogDensityTable[i] == 127) ? 255 : renderState.fogDensityTable[i] << 1;
			}
			
			glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_LookupTable);
			glBindTexture(GL_TEXTURE_1D, OGLRef.texFogDensityTableID);
			glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 32, GL_RED, GL_UNSIGNED_BYTE, fogDensityTable);
		}
		
		if (this->_enableEdgeMark && this->_deviceInfo.isEdgeMarkSupported)
		{
			const u8 alpha8 = (renderState.DISP3DCNT.EnableAntialiasing) ? 0x80 : 0xFF;
			Color4u8 edgeColor32[8];
			
			for (size_t i = 0; i < 8; i++)
			{
				edgeColor32[i].value = COLOR555TO8888(renderState.edgeMarkColorTable[i] & 0x7FFF, alpha8);
			}
			
			glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_LookupTable);
			glBindTexture(GL_TEXTURE_1D, OGLRef.texEdgeColorTableID);
			glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 8, GL_RGBA, OGL_TEXTURE_SRC_EDGE_COLOR, edgeColor32);
		}
		
		// Setup render states
		this->_geometryProgramFlags.value = 0;
		this->_geometryProgramFlags.EnableWDepth = renderState.SWAP_BUFFERS.DepthMode;
		this->_geometryProgramFlags.EnableAlphaTest = renderState.DISP3DCNT.EnableAlphaTest;
		this->_geometryProgramFlags.EnableTextureSampling = (this->_enableTextureSampling) ? 1 : 0;
		this->_geometryProgramFlags.ToonShadingMode = renderState.DISP3DCNT.PolygonShading;
		this->_geometryProgramFlags.EnableFog = (this->_enableFog && this->_deviceInfo.isFogSupported) ? 1 : 0;
		this->_geometryProgramFlags.EnableEdgeMark = (this->_enableEdgeMark && this->_deviceInfo.isEdgeMarkSupported) ? 1 : 0;
		this->_geometryProgramFlags.OpaqueDrawMode = (this->_isDepthLEqualPolygonFacingSupported) ? 1 : 0;
		
		this->_SetupGeometryShaders(this->_geometryProgramFlags);
		
		if (renderNeedsToonTable)
		{
			glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_LookupTable);
			glBindTexture(GL_TEXTURE_1D, OGLRef.texToonTableID);
			
			if (OGL_TEXTURE_SRC_TOON_TABLE == GL_UNSIGNED_BYTE)
			{
				ColorspaceConvertBuffer5551To8888<false, false, BESwapDst>(renderState.toonTable16, OGLRef.toonTable32, 32);
				glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 32, GL_RGBA, OGL_TEXTURE_SRC_TOON_TABLE, OGLRef.toonTable32);
			}
			else
			{
				glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 32, GL_RGBA, OGL_TEXTURE_SRC_TOON_TABLE, renderState.toonTable16);
			}
		}
	}
	else
	{
		if (this->isFBOSupported)
		{
			// Even with no polygons to draw, we always need to set these 3 flags so that
			// glDrawBuffers() can reference the correct set of FBO attachments using
			// OGLGeometryFlags.DrawBuffersMode.
			this->_geometryProgramFlags.EnableFog = (this->_enableFog) ? 1 : 0;
			this->_geometryProgramFlags.EnableEdgeMark = (this->_enableEdgeMark) ? 1 : 0;
			this->_geometryProgramFlags.OpaqueDrawMode = 1;
		}
	}
	
	glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
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
	
	glUniform2f(OGLRef.uniformPolyTexScale[this->_geometryProgramFlags.value], theTexture->GetInvWidth(), theTexture->GetInvHeight());
	
	// Check if we need to use textures
	if (!theTexture->IsSamplingEnabled())
	{
		glUniform1i(OGLRef.uniformPolyEnableTexture[this->_geometryProgramFlags.value], GL_FALSE);
		glUniform1i(OGLRef.uniformTexSingleBitAlpha[this->_geometryProgramFlags.value], GL_FALSE);
		return OGLERROR_NOERR;
	}
	
	glUniform1i(OGLRef.uniformPolyEnableTexture[this->_geometryProgramFlags.value], GL_TRUE);
	glUniform1i(OGLRef.uniformTexSingleBitAlpha[this->_geometryProgramFlags.value], (packFormat != TEXMODE_A3I5 && packFormat != TEXMODE_A5I3) ? GL_TRUE : GL_FALSE);
	
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
		this->_mappedFramebuffer = (Color4u8 *__restrict)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		ENDGL();
	}
	
	this->_renderNeedsFlushMain = true;
	this->_renderNeedsFlush16 = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_1::RenderFlush(bool willFlushBuffer32, bool willFlushBuffer16)
{
	if (!this->_isPoweredOn)
	{
		return RENDER3DERROR_NOERR;
	}
	
	Color4u8 *framebufferMain = (willFlushBuffer32) ? GPU->GetEngineMain()->Get3DFramebufferMain() : NULL;
	u16 *framebuffer16 = (willFlushBuffer16) ? GPU->GetEngineMain()->Get3DFramebuffer16() : NULL;
	
	this->FlushFramebuffer(this->_mappedFramebuffer, framebufferMain, framebuffer16);
	
	return RENDER3DERROR_NOERR;
}

template size_t OpenGLRenderer::DrawPolygonsForIndexRange<OGLPolyDrawMode_DrawOpaquePolys>(const POLY *rawPolyList, const CPoly *clippedPolyList, const size_t clippedPolyCount, size_t firstIndex, size_t lastIndex, size_t &indexOffset, POLYGON_ATTR &lastPolyAttr);
template size_t OpenGLRenderer::DrawPolygonsForIndexRange<OGLPolyDrawMode_DrawTranslucentPolys>(const POLY *rawPolyList, const CPoly *clippedPolyList, const size_t clippedPolyCount, size_t firstIndex, size_t lastIndex, size_t &indexOffset, POLYGON_ATTR &lastPolyAttr);
template size_t OpenGLRenderer::DrawPolygonsForIndexRange<OGLPolyDrawMode_ZeroAlphaPass>(const POLY *rawPolyList, const CPoly *clippedPolyList, const size_t clippedPolyCount, size_t firstIndex, size_t lastIndex, size_t &indexOffset, POLYGON_ATTR &lastPolyAttr);
