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

#include "OGLRender_3_2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <sstream>

#include "utils/bits.h"
#include "common.h"
#include "debug.h"
#include "NDSSystem.h"

//------------------------------------------------------------

// Basic Functions
OGLEXT(PFNGLGETSTRINGIPROC, glGetStringi) // Core in v3.0 and ES v3.0
OGLEXT(PFNGLCLEARBUFFERFVPROC, glClearBufferfv) // Core in v3.0 and ES v3.0
OGLEXT(PFNGLCLEARBUFFERFIPROC, glClearBufferfi) // Core in v3.0 and ES v3.0

// Shaders
#if defined(GL_VERSION_3_0)
OGLEXT(PFNGLBINDFRAGDATALOCATIONPROC, glBindFragDataLocation) // Core in v3.0, not available in ES
#endif
#if defined(GL_VERSION_3_3) || defined(GL_ARB_blend_func_extended)
OGLEXT(PFNGLBINDFRAGDATALOCATIONINDEXEDPROC, glBindFragDataLocationIndexed) // Core in v3.3, not available in ES
#endif

// Buffer Objects
OGLEXT(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange) // Core in v3.0 and ES v3.0

// FBO
OGLEXT(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) // Core in v3.0 and ES v2.0
OGLEXT(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) // Core in v3.0 and ES v2.0
OGLEXT(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) // Core in v3.0 and ES v2.0
OGLEXT(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) // Core in v3.0 and ES v2.0
OGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) // Core in v3.0 and ES v2.0
OGLEXT(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) // Core in v3.0 and ES v2.0
OGLEXT(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer) // Core in v3.0 and ES v3.0

// Multisampled FBO
OGLEXT(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) // Core in v3.0 and ES v2.0
OGLEXT(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) // Core in v3.0 and ES v2.0
OGLEXT(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) // Core in v3.0 and ES v2.0
OGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, glRenderbufferStorageMultisample) // Core in v3.0 and ES v3.0
OGLEXT(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers) // Core in v3.0 and ES v2.0
#if defined(GL_VERSION_3_2)
OGLEXT(PFNGLTEXIMAGE2DMULTISAMPLEPROC, glTexImage2DMultisample) // Core in v3.2, not available in ES
#endif

// UBO
OGLEXT(PFNGLGETUNIFORMBLOCKINDEXPROC, glGetUniformBlockIndex) // Core in v3.1 and ES v3.0
OGLEXT(PFNGLUNIFORMBLOCKBINDINGPROC, glUniformBlockBinding) // Core in v3.1 and ES v3.0
OGLEXT(PFNGLBINDBUFFERBASEPROC, glBindBufferBase) // Core in v3.0 and ES v3.0
OGLEXT(PFNGLGETACTIVEUNIFORMBLOCKIVPROC, glGetActiveUniformBlockiv) // Core in v3.1 and ES v3.0

// TBO
#if defined(GL_VERSION_3_1) || defined(GL_ES_VERSION_3_2)
OGLEXT(PFNGLTEXBUFFERPROC, glTexBuffer) // Core in v3.1 and ES v3.2
#endif

// Sync Objects
OGLEXT(PFNGLFENCESYNCPROC, glFenceSync) // Core in v3.2 and ES v3.0
OGLEXT(PFNGLWAITSYNCPROC, glWaitSync) // Core in v3.2 and ES v3.0
OGLEXT(PFNGLDELETESYNCPROC, glDeleteSync) // Core in v3.2 and ES v3.0

void OGLLoadEntryPoints_3_2()
{
	// Basic Functions
	INITOGLEXT(PFNGLGETSTRINGIPROC, glGetStringi) // Core in v3.0 and ES v3.0
	INITOGLEXT(PFNGLCLEARBUFFERFVPROC, glClearBufferfv) // Core in v3.0 and ES v3.0
	INITOGLEXT(PFNGLCLEARBUFFERFIPROC, glClearBufferfi) // Core in v3.0 and ES v3.0
	
	// Shaders
#if defined(GL_VERSION_3_0)
	INITOGLEXT(PFNGLBINDFRAGDATALOCATIONPROC, glBindFragDataLocation) // Core in v3.0, not available in ES
#endif
#if defined(GL_VERSION_3_3) || defined(GL_ARB_blend_func_extended)
	INITOGLEXT(PFNGLBINDFRAGDATALOCATIONINDEXEDPROC, glBindFragDataLocationIndexed) // Core in v3.3, not available in ES
#endif
	
	// Buffer Objects
	INITOGLEXT(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange) // Core in v3.0 and ES v3.0
	
	// FBO
	INITOGLEXT(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) // Core in v3.0 and ES v2.0
	INITOGLEXT(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) // Core in v3.0 and ES v2.0
	INITOGLEXT(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) // Core in v3.0 and ES v2.0
	INITOGLEXT(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) // Core in v3.0 and ES v2.0
	INITOGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) // Core in v3.0 and ES v2.0
	INITOGLEXT(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) // Core in v3.0 and ES v2.0
	INITOGLEXT(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer) // Core in v3.0 and ES v3.0
	
	// Multisampled FBO
	INITOGLEXT(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) // Core in v3.0 and ES v2.0
	INITOGLEXT(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) // Core in v3.0 and ES v2.0
	INITOGLEXT(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) // Core in v3.0 and ES v2.0
	INITOGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, glRenderbufferStorageMultisample) // Core in v3.0 and ES v3.0
	INITOGLEXT(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers) // Core in v3.0 and ES v2.0
#if defined(GL_VERSION_3_2)
	INITOGLEXT(PFNGLTEXIMAGE2DMULTISAMPLEPROC, glTexImage2DMultisample) // Core in v3.2, not available in ES
#endif
	
	// UBO
	INITOGLEXT(PFNGLGETUNIFORMBLOCKINDEXPROC, glGetUniformBlockIndex) // Core in v3.1 and ES v3.0
	INITOGLEXT(PFNGLUNIFORMBLOCKBINDINGPROC, glUniformBlockBinding) // Core in v3.1 and ES v3.0
	INITOGLEXT(PFNGLBINDBUFFERBASEPROC, glBindBufferBase) // Core in v3.0 and ES v3.0
	INITOGLEXT(PFNGLGETACTIVEUNIFORMBLOCKIVPROC, glGetActiveUniformBlockiv) // Core in v3.1 and ES v3.0
	
	// TBO
#if defined(GL_VERSION_3_1) || defined(GL_ES_VERSION_3_2)
	INITOGLEXT(PFNGLTEXBUFFERPROC, glTexBuffer) // Core in v3.1 and ES v3.2
#endif

	// Sync Objects
	INITOGLEXT(PFNGLFENCESYNCPROC, glFenceSync) // Core in v3.2 and ES v3.0
	INITOGLEXT(PFNGLWAITSYNCPROC, glWaitSync) // Core in v3.2 and ES v3.0
	INITOGLEXT(PFNGLDELETESYNCPROC, glDeleteSync) // Core in v3.2 and ES v3.0
}

// Vertex shader for geometry, GLSL 1.50
const char *GeometryVtxShader_150 = {"\
IN_VTX_POSITION  vec4 inPosition;\n\
IN_VTX_TEXCOORD0 vec2 inTexCoord0;\n\
IN_VTX_COLOR     vec3 inColor; \n\
\n\
#if IS_USING_UBO_POLY_STATES\n\
layout (std140) uniform PolyStates\n\
{\n\
	ivec4 value[4096];\n\
} polyState;\n\
#elif IS_USING_TBO_POLY_STATES\n\
uniform isamplerBuffer PolyStates;\n\
#else\n\
uniform isampler2D PolyStates;\n\
#endif\n\
uniform int polyIndex;\n\
uniform bool polyDrawShadow;\n\
\n\
out vec2 vtxTexCoord;\n\
out vec4 vtxColor;\n\
flat out int polyEnableTexture;\n\
flat out int polyEnableFog;\n\
flat out int polyIsWireframe;\n\
flat out int polySetNewDepthForTranslucent;\n\
flat out int polyMode;\n\
flat out int polyID;\n\
flat out int texSingleBitAlpha;\n\
flat out int polyIsBackFacing;\n\
flat out int isPolyDrawable;\n\
\n\
void main()\n\
{\n\
#if IS_USING_UBO_POLY_STATES\n\
	ivec4 polyStateVec = polyState.value[polyIndex >> 2];\n\
	int polyStateBits = polyStateVec[polyIndex & 0x03];\n\
#elif IS_USING_TBO_POLY_STATES\n\
	int polyStateBits = texelFetch(PolyStates, polyIndex).r;\n\
#else\n\
	int polyStateBits = texelFetch(PolyStates, ivec2(polyIndex & 0x00FF, (polyIndex >> 8) & 0x007F), 0).r;\n\
#endif\n\
	int texSizeShiftS = (polyStateBits >> 18) & 0x07;\n\
	int texSizeShiftT = (polyStateBits >> 21) & 0x07;\n\
	\n\
	float polyAlpha = float((polyStateBits >>  8) & 0x1F) / 31.0;\n\
	vec2 polyTexScale = vec2(1.0 / float(8 << texSizeShiftS), 1.0 / float(8 << texSizeShiftT));\n\
	\n\
	polyID                        = (polyStateBits >>  0) & 0x3F;\n\
	polyMode                      = (polyStateBits >>  6) & 0x03;\n\
	polyIsWireframe               = (polyStateBits >> 13) & 0x01;\n\
	polyEnableFog                 = (polyStateBits >> 14) & 0x01;\n\
	polySetNewDepthForTranslucent = (polyStateBits >> 15) & 0x01;\n\
	polyEnableTexture             = (polyStateBits >> 16) & 0x01;\n\
	texSingleBitAlpha             = (polyStateBits >> 17) & 0x01;\n\
	polyIsBackFacing              = (polyStateBits >> 24) & 0x01;\n\
	\n\
	isPolyDrawable                = int((polyMode != 3) || polyDrawShadow);\n\
	\n\
	mat2 texScaleMtx	= mat2(	vec2(polyTexScale.x,            0.0), \n\
								vec2(           0.0, polyTexScale.y)); \n\
	\n\
	vtxTexCoord = (texScaleMtx * inTexCoord0) / 16.0;\n\
	vtxColor = vec4(inColor / 63.0, polyAlpha);\n\
	gl_Position = inPosition / 4096.0;\n\
}\n\
"};

// Fragment shader for geometry, GLSL 1.50
const char *GeometryFragShader_150 = {"\
in vec2 vtxTexCoord;\n\
in vec4 vtxColor;\n\
flat in int polyEnableTexture;\n\
flat in int polyEnableFog;\n\
flat in int polyIsWireframe;\n\
flat in int polySetNewDepthForTranslucent;\n\
flat in int polyMode;\n\
flat in int polyID;\n\
flat in int texSingleBitAlpha;\n\
flat in int polyIsBackFacing;\n\
flat in int isPolyDrawable;\n\
\n\
layout (std140) uniform RenderStates\n\
{\n\
	bool enableAntialiasing;\n\
	bool enableFogAlphaOnly;\n\
	int clearPolyID;\n\
	float clearDepth;\n\
	float alphaTestRef;\n\
	float fogOffset;\n\
	float fogStep;\n\
	float pad_0;\n\
	vec4 fogColor;\n\
	vec4 edgeColor[8];\n\
	vec4 toonColor[32];\n\
} state;\n\
\n\
uniform sampler2D texRenderObject;\n\
uniform bool texDrawOpaque;\n\
uniform bool drawModeDepthEqualsTest;\n\
uniform bool polyDrawShadow;\n\
uniform float polyDepthOffset;\n\
\n\
OUT_COLOR vec4 outFragColor;\n\
\n\
#if DRAW_MODE_OPAQUE\n\
OUT_WORKING_BUFFER vec4 outDstBackFacing;\n\
#elif USE_DEPTH_LEQUAL_POLYGON_FACING\n\
uniform sampler2D inDstBackFacing;\n\
#endif\n\
\n\
#if ENABLE_EDGE_MARK\n\
OUT_POLY_ID vec4 outPolyID;\n\
#endif\n\
#if ENABLE_FOG\n\
OUT_FOG_ATTRIBUTES vec4 outFogAttributes;\n\
#endif\n\
#if IS_CONSERVATIVE_DEPTH_SUPPORTED && (USE_NDS_DEPTH_CALCULATION || ENABLE_FOG) && !ENABLE_W_DEPTH\n\
layout (depth_less) out float gl_FragDepth;\n\
#endif\n\
\n\
void main()\n\
{\n\
#if USE_DEPTH_LEQUAL_POLYGON_FACING && !DRAW_MODE_OPAQUE\n\
	bool isOpaqueDstBackFacing = bool( texelFetch(inDstBackFacing, ivec2(gl_FragCoord.xy), 0).r );\n\
	if ( drawModeDepthEqualsTest && (bool(polyIsBackFacing) || !isOpaqueDstBackFacing) )\n\
	{\n\
		discard;\n\
	}\n\
#endif\n\
	\n\
	vec4 mainTexColor = (ENABLE_TEXTURE_SAMPLING && bool(polyEnableTexture)) ? texture(texRenderObject, vtxTexCoord) : vec4(1.0, 1.0, 1.0, 1.0);\n\
	\n\
	if (!bool(texSingleBitAlpha))\n\
	{\n\
		if (texDrawOpaque)\n\
		{\n\
			if ( (polyMode != 1) && (mainTexColor.a <= 0.999) )\n\
			{\n\
				discard;\n\
			}\n\
		}\n\
		else\n\
		{\n\
			if ( ((polyMode != 1) && (mainTexColor.a * vtxColor.a > 0.999)) || ((polyMode == 1) && (vtxColor.a > 0.999)) )\n\
			{\n\
				discard;\n\
			}\n\
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
	outFragColor = mainTexColor * vtxColor;\n\
	\n\
	if (polyMode == 1)\n\
	{\n\
		outFragColor.rgb = (ENABLE_TEXTURE_SAMPLING && bool(polyEnableTexture)) ? mix(vtxColor.rgb, mainTexColor.rgb, mainTexColor.a) : vtxColor.rgb;\n\
		outFragColor.a = vtxColor.a;\n\
	}\n\
	else if (polyMode == 2)\n\
	{\n\
		vec3 newToonColor = state.toonColor[int((vtxColor.r * 31.0) + 0.5)].rgb;\n\
#if TOON_SHADING_MODE\n\
		outFragColor.rgb = min((mainTexColor.rgb * vtxColor.r) + newToonColor.rgb, 1.0);\n\
#else\n\
		outFragColor.rgb = mainTexColor.rgb * newToonColor.rgb;\n\
#endif\n\
	}\n\
	else if ((polyMode == 3) && polyDrawShadow)\n\
	{\n\
		outFragColor = vtxColor;\n\
	}\n\
	\n\
	if ( (isPolyDrawable != 0) && ((outFragColor.a < 0.001) || (ENABLE_ALPHA_TEST && outFragColor.a < state.alphaTestRef)) )\n\
	{\n\
		discard;\n\
	}\n\
#if ENABLE_EDGE_MARK\n\
	outPolyID = (isPolyDrawable != 0) ? vec4( float(polyID)/63.0, float(polyIsWireframe == 1), 0.0, float(outFragColor.a > 0.999) ) : vec4(0.0, 0.0, 0.0, 0.0);\n\
#endif\n\
#if ENABLE_FOG\n\
	outFogAttributes = (isPolyDrawable != 0) ? vec4( float(polyEnableFog), 0.0, 0.0, float((outFragColor.a > 0.999) ? 1.0 : 0.5) ) : vec4(0.0, 0.0, 0.0, 0.0);\n\
#endif\n\
#if DRAW_MODE_OPAQUE\n\
	outDstBackFacing = vec4(float(polyIsBackFacing), 0.0, 0.0, 1.0);\n\
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

// Vertex shader for determining which pixels have a zero alpha, GLSL 1.50
const char *GeometryZeroDstAlphaPixelMaskVtxShader_150 = {"\
IN_VTX_POSITION vec2 inPosition;\n\
\n\
void main()\n\
{\n\
	gl_Position = vec4(inPosition, 0.0, 1.0);\n\
}\n\
"};

// Fragment shader for determining which pixels have a zero alpha, GLSL 1.50
const char *GeometryZeroDstAlphaPixelMaskFragShader_150 = {"\
uniform sampler2D texInFragColor;\n\
\n\
void main()\n\
{\n\
	vec4 inFragColor = texelFetch(texInFragColor, ivec2(gl_FragCoord.xy), 0);\n\
	\n\
	if (inFragColor.a <= 0.001)\n\
	{\n\
		discard;\n\
	}\n\
}\n\
"};

// Fragment shader for determining which pixels have a zero alpha, GLSL 1.50
const char *MSGeometryZeroDstAlphaPixelMaskFragShader_150 = {"\
uniform sampler2DMS texInFragColor;\n\
\n\
void main()\n\
{\n\
	vec4 inFragColor = texelFetch(texInFragColor, ivec2(gl_FragCoord.xy), gl_SampleID);\n\
	\n\
	if (inFragColor.a <= 0.001)\n\
	{\n\
		discard;\n\
	}\n\
}\n\
"};

// Vertex shader for applying edge marking, GLSL 1.50
const char *EdgeMarkVtxShader_150 = {"\
IN_VTX_POSITION  vec2 inPosition;\n\
IN_VTX_TEXCOORD0 vec2 inTexCoord0;\n\
\n\
out vec2 texCoord[5];\n\
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

// Fragment shader for applying edge marking, GLSL 1.50
const char *EdgeMarkFragShader_150 = {"\
in vec2 texCoord[5];\n\
\n\
layout (std140) uniform RenderStates\n\
{\n\
	bool enableAntialiasing;\n\
	bool enableFogAlphaOnly;\n\
	int clearPolyID;\n\
	float clearDepth;\n\
	float alphaTestRef;\n\
	float fogOffset;\n\
	float fogStep;\n\
	float pad_0;\n\
	vec4 fogColor;\n\
	vec4 edgeColor[8];\n\
	vec4 toonColor[32];\n\
} state;\n\
\n\
uniform sampler2D texInFragDepth;\n\
uniform sampler2D texInPolyID;\n\
\n\
OUT_COLOR vec4 outEdgeColor;\n\
\n\
void main()\n\
{\n\
	vec4 polyIDInfo[5];\n\
	polyIDInfo[0] = texture(texInPolyID, texCoord[0]);\n\
	polyIDInfo[1] = texture(texInPolyID, texCoord[1]);\n\
	polyIDInfo[2] = texture(texInPolyID, texCoord[2]);\n\
	polyIDInfo[3] = texture(texInPolyID, texCoord[3]);\n\
	polyIDInfo[4] = texture(texInPolyID, texCoord[4]);\n\
	\n\
	float depth[5];\n\
	depth[0] = texture(texInFragDepth, texCoord[0]).r;\n\
	depth[1] = texture(texInFragDepth, texCoord[1]).r;\n\
	depth[2] = texture(texInFragDepth, texCoord[2]).r;\n\
	depth[3] = texture(texInFragDepth, texCoord[3]).r;\n\
	depth[4] = texture(texInFragDepth, texCoord[4]).r;\n\
	\n\
	bool isWireframe[5];\n\
	isWireframe[0] = bool(polyIDInfo[0].g);\n\
	\n\
	outEdgeColor = vec4(0.0, 0.0, 0.0, 0.0);\n\
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
		bool isEdgeMarkingClearValues = ((polyID[0] != state.clearPolyID) && (depth[0] < state.clearDepth) && !isWireframe[0]);\n\
		\n\
		if ( ((gl_FragCoord.x >= FRAMEBUFFER_SIZE_X-1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[1]) && (depth[0] >= depth[1]) && !isWireframe[1])) )\n\
		{\n\
			outEdgeColor = (gl_FragCoord.x >= FRAMEBUFFER_SIZE_X-1.0) ? state.edgeColor[polyID[0]/8] : state.edgeColor[polyID[1]/8];\n\
		}\n\
		else if ( ((gl_FragCoord.y >= FRAMEBUFFER_SIZE_Y-1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[2]) && (depth[0] >= depth[2]) && !isWireframe[2])) )\n\
		{\n\
			outEdgeColor = (gl_FragCoord.y >= FRAMEBUFFER_SIZE_Y-1.0) ? state.edgeColor[polyID[0]/8] : state.edgeColor[polyID[2]/8];\n\
		}\n\
		else if ( ((gl_FragCoord.x < 1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[3]) && (depth[0] >= depth[3]) && !isWireframe[3])) )\n\
		{\n\
			outEdgeColor = (gl_FragCoord.x < 1.0) ? state.edgeColor[polyID[0]/8] : state.edgeColor[polyID[3]/8];\n\
		}\n\
		else if ( ((gl_FragCoord.y < 1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[4]) && (depth[0] >= depth[4]) && !isWireframe[4])) )\n\
		{\n\
			outEdgeColor = (gl_FragCoord.y < 1.0) ? state.edgeColor[polyID[0]/8] : state.edgeColor[polyID[4]/8];\n\
		}\n\
	}\n\
}\n\
"};

// Vertex shader for applying edge marking, GLSL 1.50
const char *MSEdgeMarkVtxShader_150 = {"\
IN_VTX_POSITION  vec2 inPosition;\n\
IN_VTX_TEXCOORD0 vec2 inTexCoord0;\n\
\n\
void main()\n\
{\n\
	gl_Position = vec4(inPosition, 0.0, 1.0);\n\
}\n\
"};

// Fragment shader for applying edge marking, GLSL 1.50
const char *MSEdgeMarkFragShader_150 = {"\
layout (std140) uniform RenderStates\n\
{\n\
	bool enableAntialiasing;\n\
	bool enableFogAlphaOnly;\n\
	int clearPolyID;\n\
	float clearDepth;\n\
	float alphaTestRef;\n\
	float fogOffset;\n\
	float fogStep;\n\
	float pad_0;\n\
	vec4 fogColor;\n\
	vec4 edgeColor[8];\n\
	vec4 toonColor[32];\n\
} state;\n\
\n\
uniform sampler2DMS texInFragDepth;\n\
uniform sampler2DMS texInPolyID;\n\
\n\
OUT_COLOR vec4 outEdgeColor;\n\
\n\
void main()\n\
{\n\
	vec4 polyIDInfo[5];\n\
	polyIDInfo[0] = texelFetch(texInPolyID, ivec2(gl_FragCoord.xy) + ivec2( 0, 0), gl_SampleID);\n\
	polyIDInfo[1] = texelFetch(texInPolyID, ivec2(gl_FragCoord.xy) + ivec2( 1, 0), gl_SampleID);\n\
	polyIDInfo[2] = texelFetch(texInPolyID, ivec2(gl_FragCoord.xy) + ivec2( 0, 1), gl_SampleID);\n\
	polyIDInfo[3] = texelFetch(texInPolyID, ivec2(gl_FragCoord.xy) + ivec2(-1, 0), gl_SampleID);\n\
	polyIDInfo[4] = texelFetch(texInPolyID, ivec2(gl_FragCoord.xy) + ivec2( 0,-1), gl_SampleID);\n\
	\n\
	float depth[5];\n\
	depth[0] = texelFetch(texInFragDepth, ivec2(gl_FragCoord.xy) + ivec2( 0, 0), gl_SampleID).r;\n\
	depth[1] = texelFetch(texInFragDepth, ivec2(gl_FragCoord.xy) + ivec2( 1, 0), gl_SampleID).r;\n\
	depth[2] = texelFetch(texInFragDepth, ivec2(gl_FragCoord.xy) + ivec2( 0, 1), gl_SampleID).r;\n\
	depth[3] = texelFetch(texInFragDepth, ivec2(gl_FragCoord.xy) + ivec2(-1, 0), gl_SampleID).r;\n\
	depth[4] = texelFetch(texInFragDepth, ivec2(gl_FragCoord.xy) + ivec2( 0,-1), gl_SampleID).r;\n\
	\n\
	bool isWireframe[5];\n\
	isWireframe[0] = bool(polyIDInfo[0].g);\n\
	\n\
	outEdgeColor = vec4(0.0, 0.0, 0.0, 0.0);\n\
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
		bool isEdgeMarkingClearValues = ((polyID[0] != state.clearPolyID) && (depth[0] < state.clearDepth) && !isWireframe[0]);\n\
		\n\
		if ( ((gl_FragCoord.x >= FRAMEBUFFER_SIZE_X-1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[1]) && (depth[0] >= depth[1]) && !isWireframe[1])) )\n\
		{\n\
			outEdgeColor = (gl_FragCoord.x >= FRAMEBUFFER_SIZE_X-1.0) ? state.edgeColor[polyID[0]/8] : state.edgeColor[polyID[1]/8];\n\
		}\n\
		else if ( ((gl_FragCoord.y >= FRAMEBUFFER_SIZE_Y-1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[2]) && (depth[0] >= depth[2]) && !isWireframe[2])) )\n\
		{\n\
			outEdgeColor = (gl_FragCoord.y >= FRAMEBUFFER_SIZE_Y-1.0) ? state.edgeColor[polyID[0]/8] : state.edgeColor[polyID[2]/8];\n\
		}\n\
		else if ( ((gl_FragCoord.x < 1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[3]) && (depth[0] >= depth[3]) && !isWireframe[3])) )\n\
		{\n\
			outEdgeColor = (gl_FragCoord.x < 1.0) ? state.edgeColor[polyID[0]/8] : state.edgeColor[polyID[3]/8];\n\
		}\n\
		else if ( ((gl_FragCoord.y < 1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[4]) && (depth[0] >= depth[4]) && !isWireframe[4])) )\n\
		{\n\
			outEdgeColor = (gl_FragCoord.y < 1.0) ? state.edgeColor[polyID[0]/8] : state.edgeColor[polyID[4]/8];\n\
		}\n\
	}\n\
}\n\
"};

// Vertex shader for applying fog, GLSL 1.50
const char *FogVtxShader_150 = {"\
IN_VTX_POSITION vec2 inPosition;\n\
IN_VTX_TEXCOORD0 vec2 inTexCoord0;\n\
out vec2 texCoord;\n\
\n\
void main()\n\
{\n\
	texCoord = inTexCoord0;\n\
	gl_Position = vec4(inPosition, 0.0, 1.0);\n\
}\n\
"};

// Fragment shader for applying fog, GLSL 1.50
const char *FogFragShader_150 = {"\
layout (std140) uniform RenderStates\n\
{\n\
	bool enableAntialiasing;\n\
	bool enableFogAlphaOnly;\n\
	int clearPolyID;\n\
	float clearDepth;\n\
	float alphaTestRef;\n\
	float fogOffset;\n\
	float fogStep;\n\
	float pad_0;\n\
	vec4 fogColor;\n\
	vec4 edgeColor[8];\n\
	vec4 toonColor[32];\n\
} state;\n\
\n\
in vec2 texCoord;\n\
\n\
uniform sampler2D texInFragDepth;\n\
uniform sampler2D texInFogAttributes;\n\
uniform sampler2D texFogDensityTable;\n\
\n\
OUT_COLOR vec4 outFogWeight;\n\
\n\
void main()\n\
{\n\
	float inFragDepth = texture(texInFragDepth, texCoord).r;\n\
	vec4 inFogAttributes = texture(texInFogAttributes, texCoord);\n\
	bool polyEnableFog = (inFogAttributes.r > 0.999);\n\
	outFogWeight = vec4(0.0);\n\
	\n\
	if (polyEnableFog)\n\
	{\n\
		float densitySelect = (FOG_STEP == 0) ? ((inFragDepth <= FOG_OFFSETF) ? 0.0 : 1.0) : (inFragDepth * (1024.0/float(FOG_STEP))) + (((-float(FOG_OFFSET)/float(FOG_STEP)) - 0.5) / 32.0);\n\
		float fogMixWeight = texture( texFogDensityTable, vec2(densitySelect, 0.0) ).r;\n\
		outFogWeight = (state.enableFogAlphaOnly) ? vec4(vec3(0.0), fogMixWeight) : vec4(fogMixWeight);\n\
	}\n\
}\n\
"};

// Vertex shader for applying fog, GLSL 1.50
const char *MSFogVtxShader_150 = {"\
IN_VTX_POSITION vec2 inPosition;\n\
IN_VTX_TEXCOORD0 vec2 inTexCoord0;\n\
\n\
void main()\n\
{\n\
	gl_Position = vec4(inPosition, 0.0, 1.0);\n\
}\n\
"};

// Fragment shader for applying fog, GLSL 1.50
const char *MSFogFragShader_150 = {"\
layout (std140) uniform RenderStates\n\
{\n\
	bool enableAntialiasing;\n\
	bool enableFogAlphaOnly;\n\
	int clearPolyID;\n\
	float clearDepth;\n\
	float alphaTestRef;\n\
	float fogOffset;\n\
	float fogStep;\n\
	float pad_0;\n\
	vec4 fogColor;\n\
	vec4 edgeColor[8];\n\
	vec4 toonColor[32];\n\
} state;\n\
\n\
uniform sampler2DMS texInFragDepth;\n\
uniform sampler2DMS texInFogAttributes;\n\
uniform sampler2D texFogDensityTable;\n\
\n\
OUT_COLOR vec4 outFogWeight;\n\
\n\
void main()\n\
{\n\
	float inFragDepth = texelFetch(texInFragDepth, ivec2(gl_FragCoord.xy), gl_SampleID).r;\n\
	vec4 inFogAttributes = texelFetch(texInFogAttributes, ivec2(gl_FragCoord.xy), gl_SampleID);\n\
	bool polyEnableFog = (inFogAttributes.r > 0.999);\n\
	outFogWeight = vec4(0.0);\n\
	\n\
	if (polyEnableFog)\n\
	{\n\
		float densitySelect = (FOG_STEP == 0) ? ((inFragDepth <= FOG_OFFSETF) ? 0.0 : 1.0) : (inFragDepth * (1024.0/float(FOG_STEP))) + (((-float(FOG_OFFSET)/float(FOG_STEP)) - 0.5) / 32.0);\n\
		float fogMixWeight = texture( texFogDensityTable, vec2(densitySelect, 0.0) ).r;\n\
		outFogWeight = (state.enableFogAlphaOnly) ? vec4(vec3(0.0), fogMixWeight) : vec4(fogMixWeight);\n\
	}\n\
}\n\
"};

// Vertex shader for the final framebuffer, GLSL 1.50
const char *FramebufferOutputVtxShader_150 = {"\
IN_VTX_POSITION vec2 inPosition;\n\
IN_VTX_TEXCOORD0 vec2 inTexCoord0;\n\
out vec2 texCoord;\n\
\n\
void main()\n\
{\n\
	texCoord = vec2(inTexCoord0.x, 1.0 - inTexCoord0.y);\n\
	gl_Position = vec4(inPosition, 0.0, 1.0);\n\
}\n\
"};

// Fragment shader for the final RGBA6665 formatted framebuffer, GLSL 1.50
const char *FramebufferOutput6665FragShader_150 = {"\
in vec2 texCoord;\n\
\n\
uniform sampler2D texInFragColor;\n\
\n\
OUT_COLOR vec4 outFragColor6665;\n\
\n\
void main()\n\
{\n\
	outFragColor6665     = texture(texInFragColor, texCoord);\n\
	outFragColor6665     = floor((outFragColor6665 * 255.0) + 0.5);\n\
	outFragColor6665.rgb = floor(outFragColor6665.rgb / 4.0);\n\
	outFragColor6665.a   = floor(outFragColor6665.a   / 8.0);\n\
	\n\
	outFragColor6665 /= 255.0;\n\
}\n\
"};

void OGLCreateRenderer_3_2(OpenGLRenderer **rendererPtr)
{
	if (IsOpenGLDriverVersionSupported(3, 2, 0))
	{
		*rendererPtr = new OpenGLRenderer_3_2;
		(*rendererPtr)->SetVersion(3, 2, 0);
	}
}

OpenGLRenderer_3_2::OpenGLRenderer_3_2()
{
	_variantID = OpenGLVariantID_CoreProfile_3_2;
	_is64kUBOSupported = false;
	_isTBOSupported = false;
	_isShaderFixedLocationSupported = false;
	_isConservativeDepthSupported = false;
	_isConservativeDepthAMDSupported = false;
	_syncBufferSetup = NULL;
}

OpenGLRenderer_3_2::~OpenGLRenderer_3_2()
{
	glFinish();
	
	glUseProgram(0);
	this->DestroyMSGeometryZeroDstAlphaProgram();
	
	this->DestroyVAOs();
	this->DestroyFBOs();
	this->DestroyMultisampledFBO();
}

Render3DError OpenGLRenderer_3_2::InitExtensions()
{
	OGLRenderRef &OGLRef = *this->ref;
	Render3DError error = OGLERROR_NOERR;
	
	// Get OpenGL extensions
	std::set<std::string> oglExtensionSet;
	this->GetExtensionSet(&oglExtensionSet);
	
	// Get host GPU device properties
	GLint maxUBOSize = 0;
	glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUBOSize);
	this->_is64kUBOSupported = (maxUBOSize >= 65536);
	
	// TBOs should always be supported in 3.2 Core Profile.
	this->_isTBOSupported = true;
	
	// Fixed locations in shaders are only supported in v3.3 and later.
	this->_isShaderFixedLocationSupported = IsOpenGLDriverVersionSupported(3, 3, 0);
	
	GLfloat maxAnisotropyOGL = 1.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropyOGL);
	this->_deviceInfo.maxAnisotropy = (float)maxAnisotropyOGL;
	
	// OpenGL 3.2 should be able to handle the GL_RGBA format in glReadPixels without any performance penalty.
	OGLRef.readPixelsBestFormat = GL_RGBA;
	OGLRef.readPixelsBestDataType = GL_UNSIGNED_BYTE;
	
	this->_deviceInfo.isEdgeMarkSupported = true;
	this->_deviceInfo.isFogSupported = true;
	
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
	
	// OpenGL v3.2 Core Profile should have all the necessary features to be able to flip and convert the framebuffer.
	this->willFlipAndConvertFramebufferOnGPU = true;
	
	this->_isSampleShadingSupported = this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_sample_shading");
	this->_isConservativeDepthSupported = this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_conservative_depth") && IsOpenGLDriverVersionSupported(4, 0, 0);
	this->_isConservativeDepthAMDSupported = this->IsExtensionPresent(&oglExtensionSet, "GL_AMD_conservative_depth") && IsOpenGLDriverVersionSupported(4, 0, 0);
	
	this->_enableTextureSmoothing = CommonSettings.GFX3D_Renderer_TextureSmoothing;
	this->_emulateShadowPolygon = CommonSettings.OpenGL_Emulation_ShadowPolygon;
	this->_emulateSpecialZeroAlphaBlending = CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending;
	this->_emulateNDSDepthCalculation = CommonSettings.OpenGL_Emulation_NDSDepthCalculation;
	this->_emulateDepthLEqualPolygonFacing = CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing;
	
	// Load and create shaders. Return on any error, since v3.2 Core Profile makes shaders mandatory.
	this->isShaderSupported	= true;
	
	error = this->CreateGeometryPrograms();
	if (error != OGLERROR_NOERR)
	{
		glUseProgram(0);
		this->DestroyGeometryPrograms();
		this->isShaderSupported = false;
		
		return error;
	}
	
	error = this->CreateGeometryZeroDstAlphaProgram(GeometryZeroDstAlphaPixelMaskVtxShader_150, GeometryZeroDstAlphaPixelMaskFragShader_150);
	if (error != OGLERROR_NOERR)
	{
		glUseProgram(0);
		this->DestroyGeometryPrograms();
		this->isShaderSupported = false;
		
		return error;
	}
	
	if (this->_isSampleShadingSupported)
	{
		error = this->CreateMSGeometryZeroDstAlphaProgram(GeometryZeroDstAlphaPixelMaskVtxShader_150, MSGeometryZeroDstAlphaPixelMaskFragShader_150);
		this->_isSampleShadingSupported = (error == OGLERROR_NOERR);
		
		error = this->CreateEdgeMarkProgram(true, MSEdgeMarkVtxShader_150, MSEdgeMarkFragShader_150);
		this->_isSampleShadingSupported = this->_isSampleShadingSupported && (error == OGLERROR_NOERR);
	}
	
	INFO("OpenGL: Successfully created geometry shaders.\n");
	error = this->InitPostprocessingPrograms(EdgeMarkVtxShader_150,
	                                         EdgeMarkFragShader_150,
	                                         FramebufferOutputVtxShader_150,
	                                         FramebufferOutput6665FragShader_150,
	                                         NULL);
	if (error != OGLERROR_NOERR)
	{
		glUseProgram(0);
		this->DestroyGeometryPrograms();
		this->DestroyGeometryZeroDstAlphaProgram();
		this->DestroyMSGeometryZeroDstAlphaProgram();
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
	
	this->_isFBOBlitSupported = true;
	this->isMultisampledFBOSupported = true;
	this->_selectedMultisampleSize = CommonSettings.GFX3D_Renderer_MultisampleSize;
	
	GLint maxSamplesOGL = 0;
	glGetIntegerv(GL_MAX_SAMPLES, &maxSamplesOGL);
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
	
	this->_isDepthLEqualPolygonFacingSupported = true;
	this->_enableMultisampledRendering = ((this->_selectedMultisampleSize >= 2) && this->isMultisampledFBOSupported);
	this->_willUseMultisampleShaders = this->_isSampleShadingSupported && this->_enableMultisampledRendering;
	
	this->InitFinalRenderStates(&oglExtensionSet); // This must be done last
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::CreatePBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenBuffers(1, &OGLRef.pboRenderDataID);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, OGLRef.pboRenderDataID);
	glBufferData(GL_PIXEL_PACK_BUFFER, this->_framebufferColorSizeBytes, NULL, GL_STREAM_READ);
	this->_mappedFramebuffer = (Color4u8 *__restrict)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, this->_framebufferColorSizeBytes, GL_MAP_READ_BIT);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::CreateFBOs()
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, tempClearImageBuffer);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIDepthStencilID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, tempClearImageBuffer);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIFogAttrID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, tempClearImageBuffer);
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	// Set up FBOs
	glGenFramebuffers(1, &OGLRef.fboClearImageID);
	glGenFramebuffers(1, &OGLRef.fboRenderID);
	glGenFramebuffers(1, &OGLRef.fboRenderMutableID);
	glGenFramebuffers(1, &OGLRef.fboColorOutMainID);
	glGenFramebuffers(1, &OGLRef.fboColorOutWorkingID);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboColorOutMainID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, OGLRef.texGColorID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, OGLRef.texFinalColorID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, OGLRef.texGDepthStencilID, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to create FBOs!\n");
		this->DestroyFBOs();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	// Assign the default read/draw buffers.
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboColorOutWorkingID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, OGLRef.texFinalColorID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, OGLRef.texGColorID, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to create FBOs!\n");
		this->DestroyFBOs();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	// Assign the default read/draw buffers.
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboClearImageID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_CI_COLOROUT_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texCIColorID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_CI_FOGATTRIBUTES_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texCIFogAttrID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, OGLRef.texCIDepthStencilID, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to create FBOs!\n");
		this->DestroyFBOs();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	// Assign the default read/draw buffers.
	glReadBuffer(OGL_CI_COLOROUT_ATTACHMENT_ID);
	glDrawBuffer(GL_NONE);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboRenderMutableID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_COLOROUT_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texGColorID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_POLYID_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texGPolyID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_FOGATTRIBUTES_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texGFogAttrID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_WORKING_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texFinalColorID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, OGLRef.texGDepthStencilID, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to create FBOs!\n");
		this->DestroyFBOs();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	// Assign the default read/draw buffers.
	glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboRenderID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_COLOROUT_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texGColorID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_POLYID_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texGPolyID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_FOGATTRIBUTES_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texGFogAttrID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_WORKING_ATTACHMENT_ID, GL_TEXTURE_2D, OGLRef.texFinalColorID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, OGLRef.texGDepthStencilID, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to create FBOs!\n");
		this->DestroyFBOs();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	// Assign the default read/draw buffers.
	glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	
	OGLRef.selectedRenderingFBO = OGLRef.fboRenderID;
	OGLRef.selectedRenderingMutableFBO = OGLRef.fboRenderMutableID;
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
	glDeleteFramebuffers(1, &OGLRef.fboRenderMutableID);
	glDeleteFramebuffers(1, &OGLRef.fboColorOutMainID);
	glDeleteFramebuffers(1, &OGLRef.fboColorOutWorkingID);
	glDeleteTextures(1, &OGLRef.texCIColorID);
	glDeleteTextures(1, &OGLRef.texCIFogAttrID);
	glDeleteTextures(1, &OGLRef.texCIDepthStencilID);
	glDeleteTextures(1, &OGLRef.texGColorID);
	glDeleteTextures(1, &OGLRef.texGPolyID);
	glDeleteTextures(1, &OGLRef.texGFogAttrID);
	glDeleteTextures(1, &OGLRef.texGDepthStencilID);
	
	OGLRef.fboClearImageID = 0;
	OGLRef.fboRenderID = 0;
	OGLRef.fboColorOutMainID = 0;
	OGLRef.fboColorOutWorkingID = 0;
	OGLRef.texCIColorID = 0;
	OGLRef.texCIFogAttrID = 0;
	OGLRef.texCIDepthStencilID = 0;
	OGLRef.texGColorID = 0;
	OGLRef.texGPolyID = 0;
	OGLRef.texGFogAttrID = 0;
	OGLRef.texGDepthStencilID = 0;
	
	this->isFBOSupported = false;
}

Render3DError OpenGLRenderer_3_2::CreateMultisampledFBO(GLsizei numSamples)
{
	OGLRenderRef &OGLRef = *this->ref;
	
#ifdef GL_VERSION_3_2
	if (this->_isSampleShadingSupported)
	{
		glGenTextures(1, &OGLRef.texMSGColorID);
		glGenTextures(1, &OGLRef.texMSGWorkingID);
		glGenTextures(1, &OGLRef.texMSGDepthStencilID);
		glGenTextures(1, &OGLRef.texMSGPolyID);
		glGenTextures(1, &OGLRef.texMSGFogAttrID);
		
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GColor);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGColorID);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, GL_TRUE);
		
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FinalColor);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGWorkingID);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, GL_TRUE);
		
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_DepthStencil);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGDepthStencilID);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_DEPTH24_STENCIL8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, GL_TRUE);
		
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GPolyID);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGPolyID);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, GL_TRUE);
		
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FogAttr);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGFogAttrID);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, GL_TRUE);
		
		glActiveTexture(GL_TEXTURE0);
	}
	else
#endif
	{
		glGenRenderbuffers(1, &OGLRef.rboMSGColorID);
		glGenRenderbuffers(1, &OGLRef.rboMSGWorkingID);
		glGenRenderbuffers(1, &OGLRef.rboMSGDepthStencilID);
		glGenRenderbuffers(1, &OGLRef.rboMSGPolyID);
		glGenRenderbuffers(1, &OGLRef.rboMSGFogAttrID);
		
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGColorID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGWorkingID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGDepthStencilID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_DEPTH24_STENCIL8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGPolyID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGFogAttrID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_RGBA8, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
	}
	
	// Set up multisampled rendering FBO
	glGenFramebuffers(1, &OGLRef.fboMSIntermediateColorOutMainID);
	glGenFramebuffers(1, &OGLRef.fboMSIntermediateRenderID);
	glGenFramebuffers(1, &OGLRef.fboMSIntermediateRenderMutableID);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboMSIntermediateColorOutMainID);
	
#ifdef GL_VERSION_3_2
	if (this->_isSampleShadingSupported)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGColorID, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGWorkingID, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGDepthStencilID, 0);
	}
	else
#endif
	{
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, OGLRef.rboMSGColorID);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, OGLRef.rboMSGWorkingID);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, OGLRef.rboMSGDepthStencilID);
	}
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to create multisampled FBO. Multisample antialiasing will be disabled.\n");
		this->DestroyMultisampledFBO();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboMSIntermediateRenderID);
	
#ifdef GL_VERSION_3_2
	if (this->_isSampleShadingSupported)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_COLOROUT_ATTACHMENT_ID, GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGColorID, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_WORKING_ATTACHMENT_ID, GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGWorkingID, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGDepthStencilID, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_POLYID_ATTACHMENT_ID, GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGPolyID, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_FOGATTRIBUTES_ATTACHMENT_ID, GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGFogAttrID, 0);
	}
	else
#endif
	{
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, OGL_COLOROUT_ATTACHMENT_ID, GL_RENDERBUFFER, OGLRef.rboMSGColorID);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, OGL_WORKING_ATTACHMENT_ID, GL_RENDERBUFFER, OGLRef.rboMSGWorkingID);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, OGLRef.rboMSGDepthStencilID);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, OGL_POLYID_ATTACHMENT_ID, GL_RENDERBUFFER, OGLRef.rboMSGPolyID);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, OGL_FOGATTRIBUTES_ATTACHMENT_ID, GL_RENDERBUFFER, OGLRef.rboMSGFogAttrID);
	}
	
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to create multisampled FBO. Multisample antialiasing will be disabled.\n");
		this->DestroyMultisampledFBO();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboMSIntermediateRenderMutableID);
	
#ifdef GL_VERSION_3_2
	if (this->_isSampleShadingSupported)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_COLOROUT_ATTACHMENT_ID, GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGColorID, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_WORKING_ATTACHMENT_ID, GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGWorkingID, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGDepthStencilID, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_POLYID_ATTACHMENT_ID, GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGPolyID, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, OGL_FOGATTRIBUTES_ATTACHMENT_ID, GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGFogAttrID, 0);
	}
	else
#endif
	{
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, OGL_COLOROUT_ATTACHMENT_ID, GL_RENDERBUFFER, OGLRef.rboMSGColorID);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, OGL_WORKING_ATTACHMENT_ID, GL_RENDERBUFFER, OGLRef.rboMSGWorkingID);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, OGLRef.rboMSGDepthStencilID);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, OGL_POLYID_ATTACHMENT_ID, GL_RENDERBUFFER, OGLRef.rboMSGPolyID);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, OGL_FOGATTRIBUTES_ATTACHMENT_ID, GL_RENDERBUFFER, OGLRef.rboMSGFogAttrID);
	}
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to create multisampled FBO. Multisample antialiasing will be disabled.\n");
		this->DestroyMultisampledFBO();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	
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
	glDeleteFramebuffers(1, &OGLRef.fboMSIntermediateColorOutMainID);
	glDeleteFramebuffers(1, &OGLRef.fboMSIntermediateRenderID);
	glDeleteFramebuffers(1, &OGLRef.fboMSIntermediateRenderMutableID);
	glDeleteTextures(1, &OGLRef.texMSGColorID);
	glDeleteTextures(1, &OGLRef.texMSGWorkingID);
	glDeleteTextures(1, &OGLRef.texMSGDepthStencilID);
	glDeleteTextures(1, &OGLRef.texMSGPolyID);
	glDeleteTextures(1, &OGLRef.texMSGFogAttrID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGColorID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGWorkingID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGPolyID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGFogAttrID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGDepthStencilID);
	
	OGLRef.fboMSIntermediateColorOutMainID = 0;
	OGLRef.fboMSIntermediateRenderID = 0;
	OGLRef.fboMSIntermediateRenderMutableID = 0;
	OGLRef.texMSGColorID = 0;
	OGLRef.texMSGWorkingID = 0;
	OGLRef.texMSGDepthStencilID = 0;
	OGLRef.texMSGPolyID = 0;
	OGLRef.texMSGFogAttrID = 0;
	OGLRef.rboMSGColorID = 0;
	OGLRef.rboMSGWorkingID = 0;
	OGLRef.rboMSGPolyID = 0;
	OGLRef.rboMSGFogAttrID = 0;
	OGLRef.rboMSGDepthStencilID = 0;
	
	this->isMultisampledFBOSupported = false;
}

void OpenGLRenderer_3_2::ResizeMultisampledFBOs(GLsizei numSamples)
{
	OGLRenderRef &OGLRef = *this->ref;
	GLsizei w = (GLsizei)this->_framebufferWidth;
	GLsizei h = (GLsizei)this->_framebufferHeight;
	
	if (!this->isMultisampledFBOSupported ||
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
	
#ifdef GL_VERSION_3_2
	if (this->_isSampleShadingSupported)
	{
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GColor);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_RGBA8, w, h, GL_TRUE);
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FinalColor);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_RGBA8, w, h, GL_TRUE);
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_DepthStencil);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_DEPTH24_STENCIL8, w, h, GL_TRUE);
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GPolyID);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_RGBA8, w, h, GL_TRUE);
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FogAttr);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_RGBA8, w, h, GL_TRUE);
		glActiveTexture(GL_TEXTURE0);
	}
	else
#endif
	{
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGColorID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_RGBA8, w, h);
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGWorkingID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_RGBA8, w, h);
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGDepthStencilID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_DEPTH24_STENCIL8, w, h);
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGPolyID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_RGBA8, w, h);
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGFogAttrID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_RGBA8, w, h);
	}
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
	glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_INT, GL_FALSE, sizeof(NDSVertex), (const GLvoid *)offsetof(NDSVertex, position));
	glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_INT, GL_FALSE, sizeof(NDSVertex), (const GLvoid *)offsetof(NDSVertex, texCoord));
	glVertexAttribPointer(OGLVertexAttributeID_Color, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(NDSVertex), (const GLvoid *)offsetof(NDSVertex, color));
	
	glBindVertexArray(0);
	
	glBindVertexArray(OGLRef.vaoPostprocessStatesID);
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
	
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

Render3DError OpenGLRenderer_3_2::CreateGeometryPrograms()
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	// Create shader resources.
	if (OGLRef.uboRenderStatesID == 0)
	{
		glGenBuffers(1, &OGLRef.uboRenderStatesID);
		glBindBuffer(GL_UNIFORM_BUFFER, OGLRef.uboRenderStatesID);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(OGLRenderStates), NULL, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, OGLBindingPointID_RenderStates, OGLRef.uboRenderStatesID);
	}
	
	if (this->_is64kUBOSupported)
	{
		// Try transferring the polygon states through a UBO first. This is the fastest method,
		// but requires a GPU that supports 64k UBO transfers. Most modern GPUs should support
		// this.
		if (OGLRef.uboPolyStatesID == 0)
		{
			glGenBuffers(1, &OGLRef.uboPolyStatesID);
			glBindBuffer(GL_UNIFORM_BUFFER, OGLRef.uboPolyStatesID);
			glBufferData(GL_UNIFORM_BUFFER, MAX_CLIPPED_POLY_COUNT_FOR_UBO * sizeof(OGLPolyStates), NULL, GL_DYNAMIC_DRAW);
			glBindBufferBase(GL_UNIFORM_BUFFER, OGLBindingPointID_PolyStates, OGLRef.uboPolyStatesID);
		}
	}
#if defined(GL_VERSION_3_1) || defined(GL_ES_VERSION_3_2)
	else if (this->_isTBOSupported)
	{
		// Older GPUs that support 3.2 Core Profile but not 64k UBOs can transfer the polygon
		// states through a TBO instead. While not as fast as using a UBO, TBOs are always
		// available on any GPU that supports 3.2 Core Profile.
		if (OGLRef.tboPolyStatesID == 0)
		{
			// Set up poly states TBO
			glGenBuffers(1, &OGLRef.tboPolyStatesID);
			glBindBuffer(GL_TEXTURE_BUFFER, OGLRef.tboPolyStatesID);
			glBufferData(GL_TEXTURE_BUFFER, CLIPPED_POLYLIST_SIZE * sizeof(OGLPolyStates), NULL, GL_DYNAMIC_DRAW);
			
			glGenTextures(1, &OGLRef.texPolyStatesID);
			glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_PolyStates);
			glBindTexture(GL_TEXTURE_BUFFER, OGLRef.texPolyStatesID);
			glTexBuffer(GL_TEXTURE_BUFFER, GL_R32I, OGLRef.tboPolyStatesID);
			glActiveTexture(GL_TEXTURE0);
		}
	}
#endif
	else
	{
		// For compatibility reasons, we can transfer the polygon states through a plain old
		// integer texture. This can be useful for inheritors of this class that may not support
		// 64k UBOs or TBOs.
		glGenTextures(1, &OGLRef.texPolyStatesID);
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_PolyStates);
		glBindTexture(GL_TEXTURE_2D, OGLRef.texPolyStatesID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, 256, 128, 0, GL_RED_INTEGER, GL_INT, NULL);
		glActiveTexture(GL_TEXTURE0);
	}
	
	glGenTextures(1, &OGLRef.texFogDensityTableID);
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_LookupTable);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texFogDensityTableID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 32, 1, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glActiveTexture(GL_TEXTURE0);
	
	OGLGeometryFlags programFlags;
	programFlags.value = 0;
	
	std::stringstream shaderHeader;
	if (this->_isConservativeDepthSupported || this->_isConservativeDepthAMDSupported)
	{
		shaderHeader << "#version 400\n";
		
		// Prioritize using GL_AMD_conservative_depth over GL_ARB_conservative_depth, since AMD drivers
		// seem to have problems with GL_ARB_conservative_depth.
		shaderHeader << ((this->_isConservativeDepthAMDSupported) ? "#extension GL_AMD_conservative_depth : require\n" : "#extension GL_ARB_conservative_depth : require\n");
	}
	else if (this->_isShaderFixedLocationSupported)
	{
		shaderHeader << "#version 330\n";
	}
	else
	{
		shaderHeader << "#version 150\n";
	}
	shaderHeader << "\n";
	
	std::stringstream vsHeader;
	if (this->_isShaderFixedLocationSupported)
	{
		vsHeader << "#define IN_VTX_POSITION layout (location = "  << OGLVertexAttributeID_Position  << ") in\n";
		vsHeader << "#define IN_VTX_TEXCOORD0 layout (location = " << OGLVertexAttributeID_TexCoord0 << ") in\n";
		vsHeader << "#define IN_VTX_COLOR layout (location = "     << OGLVertexAttributeID_Color     << ") in\n";
	}
	else
	{
		vsHeader << "#define IN_VTX_POSITION in\n";
		vsHeader << "#define IN_VTX_TEXCOORD0 in\n";
		vsHeader << "#define IN_VTX_COLOR in\n";
	}
	vsHeader << "\n";
	vsHeader << "#define IS_USING_UBO_POLY_STATES " << ((OGLRef.uboPolyStatesID != 0) ? 1 : 0) << "\n";
	vsHeader << "#define IS_USING_TBO_POLY_STATES " << ((OGLRef.tboPolyStatesID != 0) ? 1 : 0) << "\n";
	vsHeader << "#define DEPTH_EQUALS_TEST_TOLERANCE " << DEPTH_EQUALS_TEST_TOLERANCE << ".0\n";
	vsHeader << "\n";
	
	std::string vtxShaderCode  = shaderHeader.str() + vsHeader.str() + std::string(GeometryVtxShader_150);
	
	std::stringstream fsHeader;
	fsHeader << "#define IS_CONSERVATIVE_DEPTH_SUPPORTED " << ((this->_isConservativeDepthSupported || this->_isConservativeDepthAMDSupported) ? 1 : 0) << "\n";
	fsHeader << "\n";
	
	for (size_t flagsValue = 0; flagsValue < 128; flagsValue++, programFlags.value++)
	{
		std::stringstream shaderFlags;
		if (this->_isShaderFixedLocationSupported)
		{
			shaderFlags << "#define OUT_COLOR layout (location = 0) out\n";
			shaderFlags << "#define OUT_WORKING_BUFFER layout (location = " << GeometryAttachmentWorkingBuffer[programFlags.DrawBuffersMode] << ") out\n";
			shaderFlags << "#define OUT_POLY_ID layout (location = "        << GeometryAttachmentPolyID[programFlags.DrawBuffersMode]        << ") out\n";
			shaderFlags << "#define OUT_FOG_ATTRIBUTES layout (location = " << GeometryAttachmentFogAttributes[programFlags.DrawBuffersMode] << ") out\n";
		}
		else
		{
			shaderFlags << "#define OUT_COLOR out\n";
			shaderFlags << "#define OUT_WORKING_BUFFER out\n";
			shaderFlags << "#define OUT_POLY_ID out\n";
			shaderFlags << "#define OUT_FOG_ATTRIBUTES out\n";
		}
		shaderFlags << "\n";
		shaderFlags << "#define USE_TEXTURE_SMOOTHING " << ((this->_enableTextureSmoothing) ? 1 : 0) << "\n";
		shaderFlags << "#define USE_NDS_DEPTH_CALCULATION " << ((this->_emulateNDSDepthCalculation) ? 1 : 0) << "\n";
		shaderFlags << "#define USE_DEPTH_LEQUAL_POLYGON_FACING " << ((this->_emulateDepthLEqualPolygonFacing) ? 1 : 0) << "\n";
		shaderFlags << "\n";
		shaderFlags << "#define ENABLE_W_DEPTH " << ((programFlags.EnableWDepth) ? 1 : 0) << "\n";
		shaderFlags << "#define ENABLE_ALPHA_TEST " << ((programFlags.EnableAlphaTest) ? "true\n" : "false\n");
		shaderFlags << "#define ENABLE_TEXTURE_SAMPLING " << ((programFlags.EnableTextureSampling) ? "true\n" : "false\n");
		shaderFlags << "#define TOON_SHADING_MODE " << ((programFlags.ToonShadingMode) ? 1 : 0) << "\n";
		shaderFlags << "#define ENABLE_FOG " << ((programFlags.EnableFog) ? 1 : 0) << "\n";
		shaderFlags << "#define ENABLE_EDGE_MARK " << ((programFlags.EnableEdgeMark) ? 1 : 0) << "\n";
		shaderFlags << "#define DRAW_MODE_OPAQUE " << ((programFlags.OpaqueDrawMode) ? 1 : 0) << "\n";
		shaderFlags << "\n";
		
		std::string fragShaderCode = shaderHeader.str() + fsHeader.str() + shaderFlags.str() + std::string(GeometryFragShader_150);
		
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
		
#if defined(GL_VERSION_3_0)
		if (!this->_isShaderFixedLocationSupported)
		{
			glBindAttribLocation(OGLRef.programGeometryID[flagsValue], OGLVertexAttributeID_Position, "inPosition");
			glBindAttribLocation(OGLRef.programGeometryID[flagsValue], OGLVertexAttributeID_TexCoord0, "inTexCoord0");
			glBindAttribLocation(OGLRef.programGeometryID[flagsValue], OGLVertexAttributeID_Color, "inColor");
			glBindFragDataLocation(OGLRef.programGeometryID[flagsValue], 0, "outFragColor");
			
			if (programFlags.EnableFog)
			{
				glBindFragDataLocation(OGLRef.programGeometryID[flagsValue], GeometryAttachmentFogAttributes[programFlags.DrawBuffersMode], "outFogAttributes");
			}
			
			if (programFlags.EnableEdgeMark)
			{
				glBindFragDataLocation(OGLRef.programGeometryID[flagsValue], GeometryAttachmentPolyID[programFlags.DrawBuffersMode], "outPolyID");
			}
			
			if (programFlags.OpaqueDrawMode)
			{
				glBindFragDataLocation(OGLRef.programGeometryID[flagsValue], GeometryAttachmentWorkingBuffer[programFlags.DrawBuffersMode], "outDstBackFacing");
			}
		}
#endif
		
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
		
		// Set up render states UBO
		const GLuint uniformBlockRenderStates			= glGetUniformBlockIndex(OGLRef.programGeometryID[flagsValue], "RenderStates");
		glUniformBlockBinding(OGLRef.programGeometryID[flagsValue], uniformBlockRenderStates, OGLBindingPointID_RenderStates);
		
		GLint uboSize = 0;
		glGetActiveUniformBlockiv(OGLRef.programGeometryID[flagsValue], uniformBlockRenderStates, GL_UNIFORM_BLOCK_DATA_SIZE, &uboSize);
		assert(uboSize == sizeof(OGLRenderStates));
		
		const GLint uniformTexRenderObject				= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "texRenderObject");
		glUniform1i(uniformTexRenderObject, 0);
		
		if (OGLRef.uboPolyStatesID != 0)
		{
			const GLuint uniformBlockPolyStates			= glGetUniformBlockIndex(OGLRef.programGeometryID[flagsValue], "PolyStates");
			glUniformBlockBinding(OGLRef.programGeometryID[flagsValue], uniformBlockPolyStates, OGLBindingPointID_PolyStates);
		}
		else
		{
			const GLint uniformTexBufferPolyStates		= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "PolyStates");
			glUniform1i(uniformTexBufferPolyStates, OGLTextureUnitID_PolyStates);
		}
		
		if (this->_emulateDepthLEqualPolygonFacing && !programFlags.OpaqueDrawMode)
		{
			const GLint uniformTexBackfacing			= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "inDstBackFacing");
			glUniform1i(uniformTexBackfacing, OGLTextureUnitID_FinalColor);
		}
		
		OGLRef.uniformTexDrawOpaque[flagsValue]           = glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "texDrawOpaque");
		OGLRef.uniformDrawModeDepthEqualsTest[flagsValue] = glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "drawModeDepthEqualsTest");
		OGLRef.uniformPolyDrawShadow[flagsValue]          = glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyDrawShadow");
		OGLRef.uniformPolyStateIndex[flagsValue]          = glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyIndex");
		OGLRef.uniformPolyDepthOffset[flagsValue]         = glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyDepthOffset");
	}
	
	return error;
}

void OpenGLRenderer_3_2::DestroyGeometryPrograms()
{
	if (!this->isShaderSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
#if defined(GL_VERSION_3_1) || defined(GL_ES_VERSION_3_2)
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
#endif
	glDeleteBuffers(1, &OGLRef.uboRenderStatesID);
	glDeleteBuffers(1, &OGLRef.uboPolyStatesID);
	glDeleteBuffers(1, &OGLRef.tboPolyStatesID);
	glDeleteTextures(1, &OGLRef.texPolyStatesID);
	
	OGLRef.uboRenderStatesID = 0;
	OGLRef.uboPolyStatesID = 0;
	OGLRef.tboPolyStatesID = 0;
	OGLRef.texPolyStatesID = 0;
	
	OpenGLRenderer_2_1::DestroyGeometryPrograms();
}

Render3DError OpenGLRenderer_3_2::CreateGeometryZeroDstAlphaProgram(const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (vtxShaderCString == NULL) || (fragShaderCString == NULL) )
	{
		return error;
	}
	
	std::stringstream shaderHeader;
	if (this->_isShaderFixedLocationSupported)
	{
		shaderHeader << "#version 330\n";
	}
	else
	{
		shaderHeader << "#version 150\n";
	}
	shaderHeader << "\n";
	
	std::stringstream vsHeader;
	if (this->_isShaderFixedLocationSupported)
	{
		vsHeader << "#define IN_VTX_POSITION layout (location = "  << OGLVertexAttributeID_Position  << ") in\n";
		vsHeader << "#define IN_VTX_TEXCOORD0 layout (location = " << OGLVertexAttributeID_TexCoord0 << ") in\n";
		vsHeader << "#define IN_VTX_COLOR layout (location = "     << OGLVertexAttributeID_Color     << ") in\n";
	}
	else
	{
		vsHeader << "#define IN_VTX_POSITION in\n";
		vsHeader << "#define IN_VTX_TEXCOORD0 in\n";
		vsHeader << "#define IN_VTX_COLOR in\n";
	}
	
	std::string vtxShaderCode  = shaderHeader.str() + vsHeader.str() + std::string(vtxShaderCString);
	std::string fragShaderCode = shaderHeader.str() + std::string(fragShaderCString);
	
	error = this->ShaderProgramCreate(OGLRef.vtxShaderGeometryZeroDstAlphaID,
									  OGLRef.fragShaderGeometryZeroDstAlphaID,
									  OGLRef.programGeometryZeroDstAlphaID,
									  vtxShaderCode.c_str(),
									  fragShaderCode.c_str());
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL: Failed to create the GEOMETRY ZERO DST ALPHA shader program.\n");
		glUseProgram(0);
		this->DestroyGeometryZeroDstAlphaProgram();
		return error;
	}
	
#if defined(GL_VERSION_3_0)
	if (!this->_isShaderFixedLocationSupported)
	{
		glBindAttribLocation(OGLRef.programGeometryZeroDstAlphaID, OGLVertexAttributeID_Position, "inPosition");
	}
#endif
	
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

Render3DError OpenGLRenderer_3_2::CreateMSGeometryZeroDstAlphaProgram(const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (vtxShaderCString == NULL) || (fragShaderCString == NULL) )
	{
		return error;
	}
	
	std::stringstream shaderHeader;
	if (this->_isShaderFixedLocationSupported)
	{
		shaderHeader << "#version 330\n";
	}
	else
	{
		shaderHeader << "#version 150\n";
	}
	shaderHeader << "#extension GL_ARB_sample_shading : require\n";
	shaderHeader << "\n";
	
	std::stringstream vsHeader;
	if (this->_isShaderFixedLocationSupported)
	{
		vsHeader << "#define IN_VTX_POSITION layout (location = "  << OGLVertexAttributeID_Position  << ") in\n";
		vsHeader << "#define IN_VTX_TEXCOORD0 layout (location = " << OGLVertexAttributeID_TexCoord0 << ") in\n";
		vsHeader << "#define IN_VTX_COLOR layout (location = "     << OGLVertexAttributeID_Color     << ") in\n";
	}
	else
	{
		vsHeader << "#define IN_VTX_POSITION in\n";
		vsHeader << "#define IN_VTX_TEXCOORD0 in\n";
		vsHeader << "#define IN_VTX_COLOR in\n";
	}
	
	std::string vtxShaderCode  = shaderHeader.str() + vsHeader.str() + std::string(vtxShaderCString);
	std::string fragShaderCode = shaderHeader.str() + std::string(fragShaderCString);
	
	error = this->ShaderProgramCreate(OGLRef.vtxShaderMSGeometryZeroDstAlphaID,
									  OGLRef.fragShaderMSGeometryZeroDstAlphaID,
									  OGLRef.programMSGeometryZeroDstAlphaID,
									  vtxShaderCode.c_str(),
									  fragShaderCode.c_str());
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL: Failed to create the MULTISAMPLED GEOMETRY ZERO DST ALPHA shader program.\n");
		glUseProgram(0);
		this->DestroyMSGeometryZeroDstAlphaProgram();
		return error;
	}
	
#if defined(GL_VERSION_3_0)
	if (!this->_isShaderFixedLocationSupported)
	{
		glBindAttribLocation(OGLRef.programMSGeometryZeroDstAlphaID, OGLVertexAttributeID_Position, "inPosition");
	}
#endif
	
	glLinkProgram(OGLRef.programMSGeometryZeroDstAlphaID);
	if (!this->ValidateShaderProgramLink(OGLRef.programMSGeometryZeroDstAlphaID))
	{
		INFO("OpenGL: Failed to link the MULTISAMPLED GEOMETRY ZERO DST ALPHA shader program.\n");
		glUseProgram(0);
		this->DestroyMSGeometryZeroDstAlphaProgram();
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.programMSGeometryZeroDstAlphaID);
	glUseProgram(OGLRef.programMSGeometryZeroDstAlphaID);
	
	const GLint uniformTexGColor = glGetUniformLocation(OGLRef.programMSGeometryZeroDstAlphaID, "texInFragColor");
	glUniform1i(uniformTexGColor, OGLTextureUnitID_GColor);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_3_2::DestroyMSGeometryZeroDstAlphaProgram()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->isShaderSupported || (OGLRef.programMSGeometryZeroDstAlphaID == 0))
	{
		return;
	}
	
	glDetachShader(OGLRef.programMSGeometryZeroDstAlphaID, OGLRef.vtxShaderMSGeometryZeroDstAlphaID);
	glDetachShader(OGLRef.programMSGeometryZeroDstAlphaID, OGLRef.fragShaderMSGeometryZeroDstAlphaID);
	glDeleteProgram(OGLRef.programMSGeometryZeroDstAlphaID);
	glDeleteShader(OGLRef.vtxShaderMSGeometryZeroDstAlphaID);
	glDeleteShader(OGLRef.fragShaderMSGeometryZeroDstAlphaID);
	
	OGLRef.programMSGeometryZeroDstAlphaID = 0;
	OGLRef.vtxShaderMSGeometryZeroDstAlphaID = 0;
	OGLRef.fragShaderMSGeometryZeroDstAlphaID = 0;
}

Render3DError OpenGLRenderer_3_2::CreateEdgeMarkProgram(const bool isMultisample, const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (vtxShaderCString == NULL) || (fragShaderCString == NULL) )
	{
		return error;
	}
	
	std::stringstream shaderHeader;
	if (this->_isShaderFixedLocationSupported)
	{
		shaderHeader << "#version 330\n";
	}
	else
	{
		shaderHeader << "#version 150\n";
	}
	
	if (isMultisample)
	{
		shaderHeader << "#extension GL_ARB_sample_shading : require\n";
	}
	
	shaderHeader << "\n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_X " << this->_framebufferWidth  << ".0 \n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_Y " << this->_framebufferHeight << ".0 \n";
	shaderHeader << "\n";
	
	std::stringstream vsHeader;
	if (this->_isShaderFixedLocationSupported)
	{
		vsHeader << "#define IN_VTX_POSITION layout (location = "  << OGLVertexAttributeID_Position  << ") in\n";
		vsHeader << "#define IN_VTX_TEXCOORD0 layout (location = " << OGLVertexAttributeID_TexCoord0 << ") in\n";
		vsHeader << "#define IN_VTX_COLOR layout (location = "     << OGLVertexAttributeID_Color     << ") in\n";
	}
	else
	{
		vsHeader << "#define IN_VTX_POSITION in\n";
		vsHeader << "#define IN_VTX_TEXCOORD0 in\n";
		vsHeader << "#define IN_VTX_COLOR in\n";
	}
	
	std::stringstream fsHeader;
	if (this->_isShaderFixedLocationSupported)
	{
		fsHeader << "#define OUT_COLOR layout (location = 0) out\n";
	}
	else
	{
		fsHeader << "#define OUT_COLOR out\n";
	}
	
	std::string vtxShaderCode  = shaderHeader.str() + vsHeader.str() + std::string(vtxShaderCString);
	std::string fragShaderCode = shaderHeader.str() + fsHeader.str() + std::string(fragShaderCString);
	GLuint programID = 0;
	
	if (isMultisample)
	{
		error = this->ShaderProgramCreate(OGLRef.vertexMSEdgeMarkShaderID,
		                                  OGLRef.fragmentMSEdgeMarkShaderID,
		                                  OGLRef.programMSEdgeMarkID,
		                                  vtxShaderCode.c_str(),
		                                  fragShaderCode.c_str());
		
		programID = OGLRef.programMSEdgeMarkID;
	}
	else
	{
		error = this->ShaderProgramCreate(OGLRef.vertexEdgeMarkShaderID,
		                                  OGLRef.fragmentEdgeMarkShaderID,
		                                  OGLRef.programEdgeMarkID,
		                                  vtxShaderCode.c_str(),
		                                  fragShaderCode.c_str());
		
		programID = OGLRef.programEdgeMarkID;
	}
	
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL: Failed to create the EDGE MARK shader program.\n");
		glUseProgram(0);
		this->DestroyEdgeMarkProgram();
		return error;
	}
	
#if defined(GL_VERSION_3_0)
	if (!this->_isShaderFixedLocationSupported)
	{
		glBindAttribLocation(programID, OGLVertexAttributeID_Position, "inPosition");
		glBindAttribLocation(programID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
		glBindFragDataLocation(programID, 0, "outEdgeColor");
	}
#endif
	
	glLinkProgram(programID);
	if (!this->ValidateShaderProgramLink(programID))
	{
		INFO("OpenGL: Failed to link the EDGE MARK shader program.\n");
		glUseProgram(0);
		this->DestroyEdgeMarkProgram();
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(programID);
	glUseProgram(programID);
	
	const GLuint uniformBlockRenderStates = glGetUniformBlockIndex(programID, "RenderStates");
	glUniformBlockBinding(programID, uniformBlockRenderStates, OGLBindingPointID_RenderStates);
	
	const GLint uniformTexGDepth  = glGetUniformLocation(programID, "texInFragDepth");
	const GLint uniformTexGPolyID = glGetUniformLocation(programID, "texInPolyID");
	glUniform1i(uniformTexGDepth, OGLTextureUnitID_DepthStencil);
	glUniform1i(uniformTexGPolyID, OGLTextureUnitID_GPolyID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::CreateFogProgram(const OGLFogProgramKey fogProgramKey, const bool isMultisample, const char *vtxShaderCString, const char *fragShaderCString)
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
	
	std::stringstream shaderHeader;
	if (this->_isShaderFixedLocationSupported)
	{
		shaderHeader << "#version 330\n";
	}
	else
	{
		shaderHeader << "#version 150\n";
	}
	
	if (isMultisample)
	{
		shaderHeader << "#extension GL_ARB_sample_shading : require\n";
	}
	
	shaderHeader << "\n";
	
	std::stringstream vsHeader;
	if (this->_isShaderFixedLocationSupported)
	{
		vsHeader << "#define IN_VTX_POSITION layout (location = "  << OGLVertexAttributeID_Position  << ") in\n";
		vsHeader << "#define IN_VTX_TEXCOORD0 layout (location = " << OGLVertexAttributeID_TexCoord0 << ") in\n";
		vsHeader << "#define IN_VTX_COLOR layout (location = "     << OGLVertexAttributeID_Color     << ") in\n";
	}
	else
	{
		vsHeader << "#define IN_VTX_POSITION in\n";
		vsHeader << "#define IN_VTX_TEXCOORD0 in\n";
		vsHeader << "#define IN_VTX_COLOR in\n";
	}
	
	std::stringstream fsHeader;
	fsHeader << "#define FOG_OFFSET " << fogOffset << "\n";
	fsHeader << "#define FOG_OFFSETF " << fogOffsetf << (((fogOffsetf == 0.0f) || (fogOffsetf == 1.0f)) ? ".0" : "") << "\n";
	fsHeader << "#define FOG_STEP " << fogStep << "\n";
	fsHeader << "\n";
	
	if (this->_isShaderFixedLocationSupported)
	{
		fsHeader << "#define OUT_COLOR layout (location = 0) out\n";
	}
	else
	{
		fsHeader << "#define OUT_COLOR out\n";
	}
	
	std::string vtxShaderCode  = shaderHeader.str() + vsHeader.str() + std::string(vtxShaderCString);
	std::string fragShaderCode = shaderHeader.str() + fsHeader.str() + std::string(fragShaderCString);
	
	OGLFogShaderID shaderID;
	shaderID.program = 0;
	shaderID.fragShader = 0;
	
	error = this->ShaderProgramCreate(OGLRef.vertexFogShaderID,
	                                  shaderID.fragShader,
	                                  shaderID.program,
	                                  vtxShaderCode.c_str(),
	                                  fragShaderCode.c_str());
	
	this->_fogProgramMap[fogProgramKey.key] = shaderID;
	
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL: Failed to create the FOG shader program.\n");
		glUseProgram(0);
		this->DestroyFogProgram(fogProgramKey);
		return error;
	}
	
#if defined(GL_VERSION_3_0)
	if (!this->_isShaderFixedLocationSupported)
	{
		glBindAttribLocation(shaderID.program, OGLVertexAttributeID_Position, "inPosition");
		glBindAttribLocation(shaderID.program, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
		glBindFragDataLocation(shaderID.program, 0, "outFogWeight");
	}
#endif
	
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
	
	const GLuint uniformBlockRenderStates = glGetUniformBlockIndex(shaderID.program, "RenderStates");
	glUniformBlockBinding(shaderID.program, uniformBlockRenderStates, OGLBindingPointID_RenderStates);
	
	const GLint uniformTexGDepth          = glGetUniformLocation(shaderID.program, "texInFragDepth");
	const GLint uniformTexGFog            = glGetUniformLocation(shaderID.program, "texInFogAttributes");
	const GLint uniformTexFogDensityTable = glGetUniformLocation(shaderID.program, "texFogDensityTable");
	glUniform1i(uniformTexGDepth, OGLTextureUnitID_DepthStencil);
	glUniform1i(uniformTexGFog, OGLTextureUnitID_FogAttr);
	glUniform1i(uniformTexFogDensityTable, OGLTextureUnitID_LookupTable);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::CreateFramebufferOutput6665Program(const size_t outColorIndex, const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (vtxShaderCString == NULL) || (fragShaderCString == NULL) )
	{
		return error;
	}
	
	std::stringstream shaderHeader;
	if (this->_isShaderFixedLocationSupported)
	{
		shaderHeader << "#version 330\n";
	}
	else
	{
		shaderHeader << "#version 150\n";
	}
	shaderHeader << "\n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_X " << this->_framebufferWidth  << ".0 \n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_Y " << this->_framebufferHeight << ".0 \n";
	shaderHeader << "\n";
	
	std::stringstream vsHeader;
	if (this->_isShaderFixedLocationSupported)
	{
		vsHeader << "#define IN_VTX_POSITION layout (location = "  << OGLVertexAttributeID_Position  << ") in\n";
		vsHeader << "#define IN_VTX_TEXCOORD0 layout (location = " << OGLVertexAttributeID_TexCoord0 << ") in\n";
		vsHeader << "#define IN_VTX_COLOR layout (location = "     << OGLVertexAttributeID_Color     << ") in\n";
	}
	else
	{
		vsHeader << "#define IN_VTX_POSITION in\n";
		vsHeader << "#define IN_VTX_TEXCOORD0 in\n";
		vsHeader << "#define IN_VTX_COLOR in\n";
	}
	
	std::stringstream fsHeader;
	if (this->_isShaderFixedLocationSupported)
	{
		fsHeader << "#define OUT_COLOR layout (location = 0) out\n";
	}
	else
	{
		fsHeader << "#define OUT_COLOR out\n";
	}
	
	std::string vtxShaderCode  = shaderHeader.str() + vsHeader.str() + std::string(vtxShaderCString);
	std::string fragShaderCode = shaderHeader.str() + fsHeader.str() + std::string(fragShaderCString);
	
	error = this->ShaderProgramCreate(OGLRef.vertexFramebufferOutput6665ShaderID,
									  OGLRef.fragmentFramebufferRGBA6665OutputShaderID,
									  OGLRef.programFramebufferRGBA6665OutputID[outColorIndex],
									  vtxShaderCode.c_str(),
									  fragShaderCode.c_str());
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL: Failed to create the FRAMEBUFFER OUTPUT RGBA6665 shader program.\n");
		glUseProgram(0);
		this->DestroyFramebufferOutput6665Programs();
		return error;
	}
	
#if defined(GL_VERSION_3_0)
	if (!this->_isShaderFixedLocationSupported)
	{
		glBindAttribLocation(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex], OGLVertexAttributeID_Position, "inPosition");
		glBindAttribLocation(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex], OGLVertexAttributeID_TexCoord0, "inTexCoord0");
		glBindFragDataLocation(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex], 0, "outFragColor6665");
	}
#endif
	
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

Render3DError OpenGLRenderer_3_2::CreateFramebufferOutput8888Program(const size_t outColorIndex, const char *vtxShaderCString, const char *fragShaderCString)
{
	OGLRenderRef &OGLRef = *this->ref;
	OGLRef.programFramebufferRGBA8888OutputID[outColorIndex] = 0;
	OGLRef.vertexFramebufferOutput8888ShaderID = 0;
	OGLRef.fragmentFramebufferRGBA8888OutputShaderID = 0;
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_3_2::GetExtensionSet(std::set<std::string> *oglExtensionSet)
{
	GLint extensionCount = 0;
	
	glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
	for (GLuint i = 0; i < extensionCount; i++)
	{
		const char * extensionName = (const char *)glGetStringi(GL_EXTENSIONS, i);
		if (extensionName == NULL) {
			continue;
		}
		oglExtensionSet->insert(std::string(extensionName));
	}
}

void OpenGLRenderer_3_2::_SetupGeometryShaders(const OGLGeometryFlags flags)
{
	const OGLRenderRef &OGLRef = *this->ref;
	
	glUseProgram(OGLRef.programGeometryID[flags.value]);
	glUniform1i(OGLRef.uniformTexDrawOpaque[flags.value], GL_FALSE);
	glUniform1i(OGLRef.uniformDrawModeDepthEqualsTest[flags.value], GL_FALSE);
	glUniform1i(OGLRef.uniformPolyDrawShadow[flags.value], GL_FALSE);
}

Render3DError OpenGLRenderer_3_2::EnableVertexAttributes()
{
	glBindVertexArray(this->ref->vaoGeometryStatesID);
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::DisableVertexAttributes()
{
	glBindVertexArray(0);
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::ZeroDstAlphaPass(const POLY *rawPolyList, const CPoly *clippedPolyList, const size_t clippedPolyCount, const size_t clippedPolyOpaqueCount, bool enableAlphaBlending, size_t indexOffset, POLYGON_ATTR lastPolyAttr)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Pre Pass: Fill in the stencil buffer based on the alpha of the current framebuffer color.
	// Fully transparent pixels (alpha == 0) -- Set stencil buffer to 0
	// All other pixels (alpha != 0) -- Set stencil buffer to 1
	
	this->DisableVertexAttributes();
	
	const bool isRunningMSAA = this->_enableMultisampledRendering && (OGLRef.selectedRenderingFBO == OGLRef.fboMSIntermediateRenderID);
	const bool isRunningMSAAWithPerSampleShading = isRunningMSAA && this->_willUseMultisampleShaders; // Doing per-sample shading will be more accurate than not doing so.
	
	if (isRunningMSAA && !isRunningMSAAWithPerSampleShading)
	{
		// Just downsample the color buffer now so that we have some texture data to sample from in the non-multisample shader.
		// This is not perfectly pixel accurate, but it's better than nothing.
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.fboColorOutMainID);
		glBlitFramebuffer(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.selectedRenderingMutableFBO);
	
	glUseProgram((isRunningMSAAWithPerSampleShading) ? OGLRef.programMSGeometryZeroDstAlphaID : OGLRef.programGeometryZeroDstAlphaID);
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
	glBindVertexArray(OGLRef.vaoPostprocessStatesID);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
	
	// Setup for multiple pass alpha poly drawing
	OGLGeometryFlags oldGProgramFlags = this->_geometryProgramFlags;
	this->_geometryProgramFlags.EnableEdgeMark = 0;
	this->_geometryProgramFlags.EnableFog = 0;
	this->_geometryProgramFlags.OpaqueDrawMode = 0;
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
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
	glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.0f, 0);
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

void OpenGLRenderer_3_2::_ResolveWorkingBackFacing()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->_emulateDepthLEqualPolygonFacing || !this->_enableMultisampledRendering || (OGLRef.selectedRenderingFBO != OGLRef.fboMSIntermediateRenderID))
	{
		return;
	}
	
	glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.fboMSIntermediateRenderID);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.fboColorOutWorkingID);
	
	glReadBuffer(OGL_WORKING_ATTACHMENT_ID);
	glBlitFramebuffer(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboMSIntermediateRenderID);
}

void OpenGLRenderer_3_2::_ResolveGeometry()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->_enableMultisampledRendering || (OGLRef.selectedRenderingFBO != OGLRef.fboMSIntermediateRenderID))
	{
		return;
	}
	
	glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.fboMSIntermediateRenderID);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.fboRenderMutableID);
	
	if (this->_enableEdgeMark)
	{
		glReadBuffer(OGL_POLYID_ATTACHMENT_ID);
		glDrawBuffer(OGL_POLYID_ATTACHMENT_ID);
		glBlitFramebuffer(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	
	if (this->_enableFog)
	{
		glReadBuffer(OGL_FOGATTRIBUTES_ATTACHMENT_ID);
		glDrawBuffer(OGL_FOGATTRIBUTES_ATTACHMENT_ID);
		glBlitFramebuffer(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	
	// Blit the color and depth buffers
	glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	glBlitFramebuffer(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	
	// Reset framebuffer targets
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboRenderID);
}

void OpenGLRenderer_3_2::_ResolveFinalFramebuffer()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->_enableMultisampledRendering || (OGLRef.selectedRenderingFBO != OGLRef.fboMSIntermediateRenderID))
	{
		return;
	}
	
	glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.fboMSIntermediateColorOutMainID);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.fboColorOutMainID);
	glBlitFramebuffer(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboColorOutMainID);
}

Render3DError OpenGLRenderer_3_2::ReadBackPixels()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->_outputFormat == NDSColorFormat_BGR666_Rev)
	{
		// Both flips and converts the framebuffer on the GPU. No additional postprocessing
		// should be necessary at this point.
		if (this->_lastTextureDrawTarget == OGLTextureUnitID_GColor)
		{
			// Use the alternate program where the output color is not at index 0.
			glUseProgram(OGLRef.programFramebufferRGBA6665OutputID[1]);
			glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboColorOutWorkingID);
			this->_lastTextureDrawTarget = OGLTextureUnitID_FinalColor;
		}
		else
		{
			// Use the program where the output color is from index 0.
			glUseProgram(OGLRef.programFramebufferRGBA6665OutputID[0]);
			glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboColorOutMainID);
			this->_lastTextureDrawTarget = OGLTextureUnitID_GColor;
		}
		
		glViewport(0, 0, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
		
		glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
		glBindVertexArray(OGLRef.vaoPostprocessStatesID);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(0);
	}
	else
	{
		// Just flips the framebuffer in Y to match the coordinates of OpenGL and the NDS hardware.
		const GLuint fboOut = (this->_lastTextureDrawTarget == OGLTextureUnitID_GColor) ? OGLRef.fboColorOutWorkingID : OGLRef.fboColorOutMainID;
		glBindFramebuffer(GL_FRAMEBUFFER, fboOut);
		glReadBuffer(GL_COLOR_ATTACHMENT1);
		glBlitFramebuffer(0, (GLint)this->_framebufferHeight, (GLint)this->_framebufferWidth, 0, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		
		this->_lastTextureDrawTarget = (this->_lastTextureDrawTarget == OGLTextureUnitID_GColor) ? OGLTextureUnitID_FinalColor : OGLTextureUnitID_GColor;
	}
	
	// Read back the pixels in RGBA format, since an OpenGL 3.2 device should be able to read back this
	// format without a performance penalty.
	if (this->_mappedFramebuffer != NULL)
	{
	   glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	   this->_mappedFramebuffer = NULL;
	}

	glReadPixels(0, 0, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, OGLRef.readPixelsBestFormat, OGLRef.readPixelsBestDataType, 0);
	
	this->_pixelReadNeedsFinish = true;
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::BeginRender(const GFX3D_State &renderState, const GFX3D_GeometryList &renderGList)
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
		
		// Copy the vertex data to the GPU asynchronously due to the potentially large upload size.
		// This buffer write will need to be synchronized before we start drawing.
		if (this->_syncBufferSetup != NULL)
		{
			glWaitSync(this->_syncBufferSetup, 0, GL_TIMEOUT_IGNORED);
			glDeleteSync(this->_syncBufferSetup);
		}
		
		const size_t vtxBufferSize = sizeof(NDSVertex) * renderGList.rawVertCount;
		NDSVertex *vtxPtr = (NDSVertex *)glMapBufferRange(GL_ARRAY_BUFFER, 0, vtxBufferSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
		memcpy(vtxPtr, renderGList.rawVtxList, vtxBufferSize);
		glUnmapBuffer(GL_ARRAY_BUFFER);
		
		this->_syncBufferSetup = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		
		// Generate the clipped polygon list.
		if ( (OGLRef.uboPolyStatesID != 0) && (this->_clippedPolyCount > MAX_CLIPPED_POLY_COUNT_FOR_UBO) )
		{
			// In practice, there shouldn't be any game scene with a clipped polygon count that
			// would exceed MAX_CLIPPED_POLY_COUNT_FOR_UBO. But if for some reason there is, then
			// we need to limit the polygon count here. Please report if this happens!
			printf("OpenGL: Clipped poly count of %d exceeds %d. Please report!!!\n", (int)this->_clippedPolyCount, MAX_CLIPPED_POLY_COUNT_FOR_UBO);
			this->_clippedPolyCount = MAX_CLIPPED_POLY_COUNT_FOR_UBO;
		}
		
		// Set up the polygon states.
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
		
		// Replace the entire buffer as a hint to the driver to orphan the buffer and avoid a synchronization cost.
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(OGLRef.vertIndexBuffer), OGLRef.vertIndexBuffer);
		
		// Set up rendering states that will remain constant for the entire frame.
		this->_pendingRenderStates.enableAntialiasing = (renderState.DISP3DCNT.EnableAntialiasing) ? GL_TRUE : GL_FALSE;
		this->_pendingRenderStates.enableFogAlphaOnly = (renderState.DISP3DCNT.FogOnlyAlpha) ? GL_TRUE : GL_FALSE;
		this->_pendingRenderStates.clearPolyID = this->_clearAttributes.opaquePolyID;
		this->_pendingRenderStates.clearDepth = (GLfloat)this->_clearAttributes.depth / (GLfloat)0x00FFFFFF;
		this->_pendingRenderStates.alphaTestRef = divide5bitBy31_LUT[renderState.alphaTestRef];
		
		if (renderNeedsToonTable)
		{
			for (size_t i = 0; i < 32; i++)
			{
				this->_pendingRenderStates.toonColor[i].r = divide5bitBy31_LUT[(renderState.toonTable16[i]      ) & 0x001F];
				this->_pendingRenderStates.toonColor[i].g = divide5bitBy31_LUT[(renderState.toonTable16[i] >>  5) & 0x001F];
				this->_pendingRenderStates.toonColor[i].b = divide5bitBy31_LUT[(renderState.toonTable16[i] >> 10) & 0x001F];
				this->_pendingRenderStates.toonColor[i].a = 1.0f;
			}
		}
		
		if (this->_enableFog)
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
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 32, 1, GL_RED, GL_UNSIGNED_BYTE, fogDensityTable);
		}
		
		if (this->_enableEdgeMark)
		{
			const GLfloat edgeColorAlpha = (renderState.DISP3DCNT.EnableAntialiasing) ? (16.0f/31.0f) : 1.0f;
			for (size_t i = 0; i < 8; i++)
			{
				this->_pendingRenderStates.edgeColor[i].r = divide5bitBy31_LUT[(renderState.edgeMarkColorTable[i]      ) & 0x001F];
				this->_pendingRenderStates.edgeColor[i].g = divide5bitBy31_LUT[(renderState.edgeMarkColorTable[i] >>  5) & 0x001F];
				this->_pendingRenderStates.edgeColor[i].b = divide5bitBy31_LUT[(renderState.edgeMarkColorTable[i] >> 10) & 0x001F];
				this->_pendingRenderStates.edgeColor[i].a = edgeColorAlpha;
			}
		}
		
		glBindBuffer(GL_UNIFORM_BUFFER, OGLRef.uboRenderStatesID);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(this->_pendingRenderStates), &this->_pendingRenderStates);
		
		OGLPolyStates *polyStates = this->_pendingPolyStates;
		
#if defined(GL_VERSION_3_1) || defined(GL_ES_VERSION_3_2)
		if (OGLRef.tboPolyStatesID != 0)
		{
			// Some drivers seem to have problems with GL_TEXTURE_BUFFER used as the target for
			// glMapBufferRange() or glBufferSubData(), causing certain polygons to intermittently
			// flicker in certain games. Therefore, we'll use glMapBuffer() in this case in order
			// to prevent these glitches from happening.
			polyStates = (OGLPolyStates *)glMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY);
		}
#endif
		
		for (size_t i = 0; i < this->_clippedPolyCount; i++)
		{
			const POLY &rawPoly = this->_rawPolyList[this->_clippedPolyList[i].index];
			
			// Get all of the polygon states that can be handled within the shader.
			const NDSTextureFormat packFormat = this->_textureList[i]->GetPackFormat();
			
			polyStates[i].packedState = 0;
			polyStates[i].PolygonID = rawPoly.attribute.PolygonID;
			polyStates[i].PolygonMode = rawPoly.attribute.Mode;
			
			polyStates[i].PolygonAlpha = (GFX3D_IsPolyWireframe(rawPoly)) ? 0x1F : rawPoly.attribute.Alpha;
			polyStates[i].IsWireframe = (GFX3D_IsPolyWireframe(rawPoly)) ? 1 : 0;
			polyStates[i].EnableFog = (rawPoly.attribute.Fog_Enable) ? 1 : 0;
			polyStates[i].SetNewDepthForTranslucent = (rawPoly.attribute.TranslucentDepthWrite_Enable) ? 1 : 0;
			
			polyStates[i].EnableTexture = (this->_textureList[i]->IsSamplingEnabled()) ? 1 : 0;
			polyStates[i].TexSingleBitAlpha = (packFormat != TEXMODE_A3I5 && packFormat != TEXMODE_A5I3) ? 1 : 0;
			polyStates[i].TexSizeShiftS = rawPoly.texParam.SizeShiftS; // Note that we are using the preshifted size of S
			polyStates[i].TexSizeShiftT = rawPoly.texParam.SizeShiftT; // Note that we are using the preshifted size of T
			
			polyStates[i].IsBackFacing = (this->_clippedPolyList[i].isPolyBackFacing) ? 1 : 0;
		}
		
		if (OGLRef.uboPolyStatesID != 0)
		{
			// Replace the entire buffer as a hint to the driver to orphan the buffer and avoid a synchronization cost.
			glBindBuffer(GL_UNIFORM_BUFFER, OGLRef.uboPolyStatesID);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, MAX_CLIPPED_POLY_COUNT_FOR_UBO * sizeof(OGLPolyStates), this->_pendingPolyStates);
		}
#if defined(GL_VERSION_3_1) || defined(GL_ES_VERSION_3_2)
		else if (OGLRef.tboPolyStatesID != 0)
		{
			glUnmapBuffer(GL_TEXTURE_BUFFER);
		}
#endif
		else
		{
			const GLsizei texH = (GLsizei)((this->_clippedPolyCount >> 8) & 0x007F) + 1;
			glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_PolyStates);
			glBindTexture(GL_TEXTURE_2D, OGLRef.texPolyStatesID); // Why is this bind necessary? Theoretically, it shouldn't be necessary, but real-world testing has proven otherwise...
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, texH, GL_RED_INTEGER, GL_INT, this->_pendingPolyStates);
		}
		
		// Set up the default draw call states.
		this->_geometryProgramFlags.value = 0;
		this->_geometryProgramFlags.EnableWDepth = renderState.SWAP_BUFFERS.DepthMode;
		this->_geometryProgramFlags.EnableAlphaTest = renderState.DISP3DCNT.EnableAlphaTest;
		this->_geometryProgramFlags.EnableTextureSampling = (this->_enableTextureSampling) ? 1 : 0;
		this->_geometryProgramFlags.ToonShadingMode = renderState.DISP3DCNT.PolygonShading;
		this->_geometryProgramFlags.EnableFog = (this->_enableFog) ? 1 : 0;
		this->_geometryProgramFlags.EnableEdgeMark = (this->_enableEdgeMark) ? 1 : 0;
		this->_geometryProgramFlags.OpaqueDrawMode = 1;
		
		this->_SetupGeometryShaders(this->_geometryProgramFlags);
	}
	else
	{
		// Even with no polygons to draw, we always need to set these 3 flags so that
		// glDrawBuffers() can reference the correct set of FBO attachments using
		// OGLGeometryFlags.DrawBuffersMode.
		this->_geometryProgramFlags.EnableFog = (this->_enableFog) ? 1 : 0;
		this->_geometryProgramFlags.EnableEdgeMark = (this->_enableEdgeMark) ? 1 : 0;
		this->_geometryProgramFlags.OpaqueDrawMode = 1;
	}
	
	if (this->_enableMultisampledRendering)
	{
		OGLRef.selectedRenderingFBO        = OGLRef.fboMSIntermediateRenderID;
		OGLRef.selectedRenderingMutableFBO = OGLRef.fboMSIntermediateRenderMutableID;
	}
	else
	{
		OGLRef.selectedRenderingFBO        = OGLRef.fboRenderID;
		OGLRef.selectedRenderingMutableFBO = OGLRef.fboRenderMutableID;
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
	glDrawBuffers(4, GeometryDrawBuffersEnum[this->_geometryProgramFlags.DrawBuffersMode]);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	
	this->_needsZeroDstAlphaPass = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::PostprocessFramebuffer()
{
	if ( (this->_clippedPolyCount < 1) ||
		(!this->_enableEdgeMark && !this->_enableFog) )
	{
		return OGLERROR_NOERR;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up the postprocessing states
	const GLuint fboPostprocess = (this->_willUseMultisampleShaders) ? OGLRef.fboMSIntermediateColorOutMainID : OGLRef.fboColorOutMainID;
	glBindFramebuffer(GL_FRAMEBUFFER, fboPostprocess);
	glViewport(0, 0, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight);
	glDisable(GL_DEPTH_TEST);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
	glBindVertexArray(OGLRef.vaoPostprocessStatesID);
	
	if (this->_enableEdgeMark)
	{
		GLuint pGZeroDstAlphaID = 0;
		GLuint pEdgeMarkID = 0;
		
		if (this->_willUseMultisampleShaders)
		{
			pGZeroDstAlphaID = OGLRef.programMSGeometryZeroDstAlphaID;
			pEdgeMarkID = OGLRef.programMSEdgeMarkID;
		}
		else
		{
			pGZeroDstAlphaID = OGLRef.programGeometryZeroDstAlphaID;
			pEdgeMarkID = OGLRef.programEdgeMarkID;
		}
		
		if (this->_needsZeroDstAlphaPass && this->_emulateSpecialZeroAlphaBlending)
		{
			// Pass 1: Determine the pixels with zero alpha
			glDrawBuffer(GL_NONE);
			glDisable(GL_BLEND);
			glEnable(GL_STENCIL_TEST);
			glStencilFunc(GL_ALWAYS, 0x40, 0x40);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
			glStencilMask(0x40);
			
			glUseProgram(pGZeroDstAlphaID);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			
			// Pass 2: Unblended edge mark colors to zero-alpha pixels
			glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
			glUseProgram(pEdgeMarkID);
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
			glUseProgram(pEdgeMarkID);
			glEnable(GL_BLEND);
			glDisable(GL_STENCIL_TEST);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
	}
	
	if (this->_enableFog)
	{
		std::map<u32, OGLFogShaderID>::iterator it = this->_fogProgramMap.find(this->_fogProgramKey.key);
		if (it == this->_fogProgramMap.end())
		{
			Render3DError error = OGLERROR_NOERR;
			
			if (this->_willUseMultisampleShaders)
			{
				error = this->CreateFogProgram(this->_fogProgramKey, true, MSFogVtxShader_150, MSFogFragShader_150);
			}
			else
			{
				error = this->CreateFogProgram(this->_fogProgramKey, false, FogVtxShader_150, FogFragShader_150);
			}
			
			if (error != OGLERROR_NOERR)
			{
				return error;
			}
		}
		
		OGLFogShaderID shaderID = this->_fogProgramMap[this->_fogProgramKey.key];
		glUseProgram(shaderID.program);
		
		glBlendFuncSeparate(GL_CONSTANT_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_CONSTANT_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		glBlendColor( this->_pendingRenderStates.fogColor.r,
					  this->_pendingRenderStates.fogColor.g,
					  this->_pendingRenderStates.fogColor.b,
					  this->_pendingRenderStates.fogColor.a );
		
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		glEnable(GL_BLEND);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_DST_ALPHA);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);
	}
	
	glBindVertexArray(0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	this->UploadClearImage(colorBuffer, depthBuffer, fogBuffer, opaquePolyID);
	
	glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.fboClearImageID);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.fboRenderMutableID);
	
	if (this->_emulateDepthLEqualPolygonFacing)
	{
		const GLfloat oglBackfacing[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		glClearBufferfv(GL_COLOR, GeometryAttachmentWorkingBuffer[this->_geometryProgramFlags.DrawBuffersMode], oglBackfacing);
	}
	
	if (this->_enableEdgeMark)
	{
		// Clear the polygon ID buffer
		const GLfloat oglPolyID[4] = {(GLfloat)opaquePolyID/63.0f, 0.0f, 0.0f, 1.0f};
		glClearBufferfv(GL_COLOR, GeometryAttachmentPolyID[this->_geometryProgramFlags.DrawBuffersMode], oglPolyID);
	}
	
	if (this->_enableFog)
	{
		// Blit the fog buffer
		glReadBuffer(OGL_CI_FOGATTRIBUTES_ATTACHMENT_ID);
		glDrawBuffer(OGL_FOGATTRIBUTES_ATTACHMENT_ID);
		glBlitFramebuffer(0, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GPU_FRAMEBUFFER_NATIVE_WIDTH, 0, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	
	// Blit the color and depth/stencil buffers. Do this last so that color attachment 0 is set to the read FBO.
	glReadBuffer(OGL_CI_COLOROUT_ATTACHMENT_ID);
	glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
	glBlitFramebuffer(0, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GPU_FRAMEBUFFER_NATIVE_WIDTH, 0, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
	
	if (this->_enableMultisampledRendering)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.fboRenderMutableID);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.fboMSIntermediateRenderMutableID);
		
		if (this->_emulateDepthLEqualPolygonFacing)
		{
			const GLfloat oglBackfacing[4] = {0.0f, 0.0f, 0.0f, 0.0f};
			glClearBufferfv(GL_COLOR, GeometryAttachmentWorkingBuffer[this->_geometryProgramFlags.DrawBuffersMode], oglBackfacing);
		}
		
		if (this->_enableEdgeMark)
		{
			const GLfloat oglPolyID[4] = {(GLfloat)opaquePolyID/63.0f, 0.0f, 0.0f, 1.0f};
			glClearBufferfv(GL_COLOR, GeometryAttachmentPolyID[this->_geometryProgramFlags.DrawBuffersMode], oglPolyID);
		}
		
		if (this->_enableFog)
		{
			glReadBuffer(OGL_FOGATTRIBUTES_ATTACHMENT_ID);
			glDrawBuffer(OGL_FOGATTRIBUTES_ATTACHMENT_ID);
			glBlitFramebuffer(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		
		// Blit the color and depth/stencil buffers. Do this last so that color attachment 0 is set to the read FBO.
		glReadBuffer(OGL_COLOROUT_ATTACHMENT_ID);
		glDrawBuffer(OGL_COLOROUT_ATTACHMENT_ID);
		glBlitFramebuffer(0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, 0, 0, (GLint)this->_framebufferWidth, (GLint)this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::ClearUsingValues(const Color4u8 &clearColor6665, const FragmentAttributes &clearAttributes)
{
	const GLfloat oglColor[4] = {divide6bitBy63_LUT[clearColor6665.r], divide6bitBy63_LUT[clearColor6665.g], divide6bitBy63_LUT[clearColor6665.b], divide5bitBy31_LUT[clearColor6665.a]};
	glClearBufferfv(GL_COLOR, 0, oglColor);
	glClearBufferfi(GL_DEPTH_STENCIL, 0, (GLfloat)clearAttributes.depth / (GLfloat)0x00FFFFFF, clearAttributes.opaquePolyID);
	
	if (this->_emulateDepthLEqualPolygonFacing)
	{
		const GLfloat oglBackfacing[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		glClearBufferfv(GL_COLOR, GeometryAttachmentWorkingBuffer[this->_geometryProgramFlags.DrawBuffersMode], oglBackfacing);
	}
	
	if (this->_enableEdgeMark)
	{
		const GLfloat oglPolyID[4] = {(GLfloat)clearAttributes.opaquePolyID/63.0f, 0.0f, 0.0f, 1.0f};
		glClearBufferfv(GL_COLOR, GeometryAttachmentPolyID[this->_geometryProgramFlags.DrawBuffersMode], oglPolyID);
	}
	
	if (this->_enableFog)
	{
		const GLfloat oglFogAttr[4] = {(GLfloat)clearAttributes.isFogged, 0.0f, 0.0f, 1.0f};
		glClearBufferfv(GL_COLOR, GeometryAttachmentFogAttributes[this->_geometryProgramFlags.DrawBuffersMode], oglFogAttr);
	}
	
	this->_needsZeroDstAlphaPass = (clearColor6665.a == 0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_3_2::SetPolygonIndex(const size_t index)
{
	this->_currentPolyIndex = index;
	glUniform1i(this->ref->uniformPolyStateIndex[this->_geometryProgramFlags.value], (GLint)index);
	
	if (this->_syncBufferSetup != NULL)
	{
		glWaitSync(this->_syncBufferSetup, 0, GL_TIMEOUT_IGNORED);
		glDeleteSync(this->_syncBufferSetup);
		this->_syncBufferSetup = NULL;
	}
}

Render3DError OpenGLRenderer_3_2::SetupPolygon(const POLY &thePoly, bool treatAsTranslucent, bool willChangeStencilBuffer, bool isBackFacing)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up depth test mode
	glDepthFunc((thePoly.attribute.DepthEqualTest_Enable) ? GL_EQUAL : GL_LESS);
	glUniform1f(OGLRef.uniformPolyDepthOffset[this->_geometryProgramFlags.value], 0.0f);
	
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
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::SetupTexture(const POLY &thePoly, size_t polyRenderIndex)
{
	OpenGLTexture *theTexture = (OpenGLTexture *)this->_textureList[polyRenderIndex];
		
	// Check if we need to use textures
	if (!theTexture->IsSamplingEnabled())
	{
		return OGLERROR_NOERR;
	}
	
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

Render3DError OpenGLRenderer_3_2::SetFramebufferSize(size_t w, size_t h)
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
	
	if (this->_mappedFramebuffer != NULL)
	{
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		glFinish();
	}
	
	const size_t newFramebufferColorSizeBytes = w * h * sizeof(Color4u8);
	glBufferData(GL_PIXEL_PACK_BUFFER, newFramebufferColorSizeBytes, NULL, GL_STREAM_READ);
	
	if (this->_mappedFramebuffer != NULL)
	{
		this->_mappedFramebuffer = (Color4u8 *__restrict)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, newFramebufferColorSizeBytes, GL_MAP_READ_BIT);
		glFinish();
	}
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FinalColor);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_DepthStencil);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, (GLsizei)w, (GLsizei)h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GColor);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GPolyID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FogAttr);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	
	glActiveTexture(GL_TEXTURE0);
	
	this->_framebufferWidth = w;
	this->_framebufferHeight = h;
	this->_framebufferPixCount = w * h;
	this->_framebufferColorSizeBytes = newFramebufferColorSizeBytes;
	this->_framebufferColor = NULL; // Don't need to make a client-side buffer since we will be reading directly from the PBO.
	
	// Recreate shaders that use the framebuffer size.
	glUseProgram(0);
	this->DestroyEdgeMarkProgram();
	this->DestroyFramebufferOutput6665Programs();
	this->DestroyMSGeometryZeroDstAlphaProgram();
	
	this->CreateEdgeMarkProgram(false, EdgeMarkVtxShader_150, EdgeMarkFragShader_150);
	
	const bool oldMultisampleShadingFlag = this->_willUseMultisampleShaders;
	if (this->_isSampleShadingSupported)
	{
		Render3DError shaderError = this->CreateMSGeometryZeroDstAlphaProgram(GeometryZeroDstAlphaPixelMaskVtxShader_150, MSGeometryZeroDstAlphaPixelMaskFragShader_150);
		this->_isSampleShadingSupported = (shaderError == OGLERROR_NOERR);
		
		shaderError = this->CreateEdgeMarkProgram(true, MSEdgeMarkVtxShader_150, MSEdgeMarkFragShader_150);
		this->_isSampleShadingSupported = this->_isSampleShadingSupported && (shaderError == OGLERROR_NOERR);
		
		this->_willUseMultisampleShaders = this->_isSampleShadingSupported && this->_enableMultisampledRendering;
	}
	
	if (this->_willUseMultisampleShaders != oldMultisampleShadingFlag)
	{
		// If, for some reason or another, this flag gets changed due to a shader compilation error,
		// then reset all fog shader programs now.
		this->DestroyFogPrograms();
	}
	
	this->CreateFramebufferOutput6665Program(0, FramebufferOutputVtxShader_150, FramebufferOutput6665FragShader_150);
	this->CreateFramebufferOutput6665Program(1, FramebufferOutputVtxShader_150, FramebufferOutput6665FragShader_150);
	
	// Call ResizeMultisampledFBOs() after _framebufferWidth and _framebufferHeight are set
	// since this method depends on them.
	GLsizei sampleSize = this->GetLimitedMultisampleSize();
	this->ResizeMultisampledFBOs(sampleSize);
	
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

Render3DError OpenGLRenderer_3_2::RenderFinish()
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
		this->_mappedFramebuffer = (Color4u8 *__restrict)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, this->_framebufferColorSizeBytes, GL_MAP_READ_BIT);
		ENDGL();
	}
	
	this->_renderNeedsFlushMain = true;
	this->_renderNeedsFlush16 = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::RenderPowerOff()
{
	OGLRenderRef &OGLRef = *this->ref;
	static const GLfloat oglColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	
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
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboColorOutMainID);
	glClearBufferfv(GL_COLOR, 0, oglColor);
	
	if (this->_mappedFramebuffer != NULL)
	{
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		this->_mappedFramebuffer = NULL;
	}
	
	glReadPixels(0, 0, (GLsizei)this->_framebufferWidth, (GLsizei)this->_framebufferHeight, OGLRef.readPixelsBestFormat, OGLRef.readPixelsBestDataType, 0);
	
	ENDGL();
	
	this->_pixelReadNeedsFinish = true;
	return OGLERROR_NOERR;
}
