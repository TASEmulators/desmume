/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2019 DeSmuME team

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
#include <algorithm>
#include <string>
#include <sstream>

#include "utils/bits.h"
#include "common.h"
#include "debug.h"
#include "NDSSystem.h"

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
OGLEXT(PFNGLTEXIMAGE2DMULTISAMPLEPROC, glTexImage2DMultisample) // Core in v3.2

// UBO
OGLEXT(PFNGLGETUNIFORMBLOCKINDEXPROC, glGetUniformBlockIndex) // Core in v3.1
OGLEXT(PFNGLUNIFORMBLOCKBINDINGPROC, glUniformBlockBinding) // Core in v3.1
OGLEXT(PFNGLBINDBUFFERBASEPROC, glBindBufferBase) // Core in v3.0
OGLEXT(PFNGLGETACTIVEUNIFORMBLOCKIVPROC, glGetActiveUniformBlockiv) // Core in v3.1

// TBO
OGLEXT(PFNGLTEXBUFFERPROC, glTexBuffer) // Core in v3.1

// Sync Objects
OGLEXT(PFNGLFENCESYNCPROC, glFenceSync) // Core in v3.2
OGLEXT(PFNGLWAITSYNCPROC, glWaitSync) // Core in v3.2
OGLEXT(PFNGLDELETESYNCPROC, glDeleteSync) // Core in v3.2

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
	INITOGLEXT(PFNGLTEXIMAGE2DMULTISAMPLEPROC, glTexImage2DMultisample)
	
	// UBO
	INITOGLEXT(PFNGLGETUNIFORMBLOCKINDEXPROC, glGetUniformBlockIndex)
	INITOGLEXT(PFNGLUNIFORMBLOCKBINDINGPROC, glUniformBlockBinding)
	INITOGLEXT(PFNGLBINDBUFFERBASEPROC, glBindBufferBase)
	INITOGLEXT(PFNGLGETACTIVEUNIFORMBLOCKIVPROC, glGetActiveUniformBlockiv)
	
	// TBO
	INITOGLEXT(PFNGLTEXBUFFERPROC, glTexBuffer)

	// Sync Objects
	INITOGLEXT(PFNGLFENCESYNCPROC, glFenceSync) // Core in v3.2
	INITOGLEXT(PFNGLWAITSYNCPROC, glWaitSync) // Core in v3.2
	INITOGLEXT(PFNGLDELETESYNCPROC, glDeleteSync) // Core in v3.2
}

// Vertex shader for geometry, GLSL 1.50
static const char *GeometryVtxShader_150 = {"\
in vec4 inPosition;\n\
in vec2 inTexCoord0;\n\
in vec3 inColor; \n\
\n\
#if IS_USING_UBO_POLY_STATES\n\
layout (std140) uniform PolyStates\n\
{\n\
	ivec4 value[4096];\n\
} polyState;\n\
#else\n\
uniform isamplerBuffer PolyStates;\n\
#endif\n\
uniform int polyIndex;\n\
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
\n\
void main()\n\
{\n\
#if IS_USING_UBO_POLY_STATES\n\
	ivec4 polyStateVec = polyState.value[polyIndex >> 2];\n\
	int polyStateBits = polyStateVec[polyIndex & 0x03];\n\
#else\n\
	int polyStateBits = texelFetch(PolyStates, polyIndex).r;\n\
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
	\n\
	mat2 texScaleMtx	= mat2(	vec2(polyTexScale.x,            0.0), \n\
								vec2(           0.0, polyTexScale.y)); \n\
	\n\
	vtxTexCoord = texScaleMtx * inTexCoord0;\n\
	vtxColor = vec4(inColor / 63.0, polyAlpha);\n\
	gl_Position = inPosition;\n\
}\n\
"};

// Fragment shader for geometry, GLSL 1.50
static const char *GeometryFragShader_150 = {"\
in vec2 vtxTexCoord;\n\
in vec4 vtxColor;\n\
flat in int polyEnableTexture;\n\
flat in int polyEnableFog;\n\
flat in int polyIsWireframe;\n\
flat in int polySetNewDepthForTranslucent;\n\
flat in int polyMode;\n\
flat in int polyID;\n\
flat in int texSingleBitAlpha;\n\
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
	float fogDensity[32];\n\
	vec4 edgeColor[8];\n\
	vec4 toonColor[32];\n\
} state;\n\
\n\
uniform sampler2D texRenderObject;\n\
uniform bool texDrawOpaque;\n\
uniform bool polyDrawShadow;\n\
uniform int polyDepthOffsetMode;\n\
\n\
out vec4 outFragColor;\n\
\n\
#if ENABLE_EDGE_MARK\n\
out vec4 outPolyID;\n\
#endif\n\
#if ENABLE_FOG\n\
out vec4 outFogAttributes;\n\
#endif\n\
#if IS_CONSERVATIVE_DEPTH_SUPPORTED && (USE_NDS_DEPTH_CALCULATION || ENABLE_FOG) && !NEEDS_DEPTH_EQUALS_TEST && !ENABLE_W_DEPTH\n\
layout (depth_less) out float gl_FragDepth;\n\
#endif\n\
\n\
void main()\n\
{\n\
	outFragColor = vec4(0.0, 0.0, 0.0, 0.0);\n\
#if ENABLE_EDGE_MARK\n\
	outPolyID = vec4(0.0, 0.0, 0.0, 0.0);\n\
#endif\n\
#if ENABLE_FOG\n\
	outFogAttributes = vec4(0.0, 0.0, 0.0, 0.0);\n\
#endif\n\
	\n\
	if ((polyMode != 3) || polyDrawShadow)\n\
	{\n\
		vec4 mainTexColor = (ENABLE_TEXTURE_SAMPLING && bool(polyEnableTexture)) ? texture(texRenderObject, vtxTexCoord) : vec4(1.0, 1.0, 1.0, 1.0);\n\
		\n\
		if (bool(texSingleBitAlpha))\n\
		{\n\
#if USE_TEXTURE_SMOOTHING\n\
			if (mainTexColor.a < 0.500)\n\
			{\n\
				mainTexColor.a = 0.0;\n\
			}\n\
			else\n\
			{\n\
				mainTexColor.rgb = mainTexColor.rgb / mainTexColor.a;\n\
				mainTexColor.a = 1.0;\n\
			}\n\
#endif\n\
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
			else\n\
			{\n\
				if ( ((polyMode != 1) && (mainTexColor.a * vtxColor.a > 0.999)) || ((polyMode == 1) && (vtxColor.a > 0.999)) )\n\
				{\n\
					discard;\n\
				}\n\
			}\n\
		}\n\
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
		else if (polyMode == 3)\n\
		{\n\
			outFragColor = vtxColor;\n\
		}\n\
		\n\
		if (outFragColor.a < 0.001 || (ENABLE_ALPHA_TEST && outFragColor.a < state.alphaTestRef))\n\
		{\n\
			discard;\n\
		}\n\
		\n\
#if ENABLE_EDGE_MARK\n\
		outPolyID = vec4( float(polyID)/63.0, float(polyIsWireframe == 1), 0.0, float(outFragColor.a > 0.999) );\n\
#endif\n\
#if ENABLE_FOG\n\
		outFogAttributes = vec4( float(polyEnableFog), 0.0, 0.0, float((outFragColor.a > 0.999) ? 1.0 : 0.5) );\n\
#endif\n\
	}\n\
	\n\
#if USE_NDS_DEPTH_CALCULATION || ENABLE_FOG\n\
	// It is tempting to perform the NDS depth calculation in the vertex shader rather than in the fragment shader.\n\
	// Resist this temptation! It is much more reliable to do the depth calculation in the fragment shader due to\n\
	// subtle interpolation differences between various GPUs and/or drivers. If the depth calculation is not done\n\
	// here, then it is very possible for the user to experience Z-fighting in certain rendering situations.\n\
	\n\
	#if NEEDS_DEPTH_EQUALS_TEST\n\
		float depthOffset = (polyDepthOffsetMode == 0) ? 0.0 : ((polyDepthOffsetMode == 1) ? -DEPTH_EQUALS_TEST_TOLERANCE : DEPTH_EQUALS_TEST_TOLERANCE);\n\
		#if ENABLE_W_DEPTH\n\
		gl_FragDepth = clamp( ( (4096.0/gl_FragCoord.w) + depthOffset ) / 16777215.0, 0.0, 1.0 );\n\
		#else\n\
		gl_FragDepth = clamp( ( (floor(gl_FragCoord.z * 4194303.0) * 4.0) + depthOffset ) / 16777215.0, 0.0, 1.0 );\n\
		#endif\n\
	#else\n\
		#if ENABLE_W_DEPTH\n\
		gl_FragDepth = (4096.0/gl_FragCoord.w) / 16777215.0;\n\
		#else\n\
		// hack: when using z-depth, drop some LSBs so that the overworld map in Dragon Quest IV shows up correctly\n\
		gl_FragDepth = (floor(gl_FragCoord.z * 4194303.0) * 4.0) / 16777215.0;\n\
		#endif\n\
	#endif\n\
#endif\n\
}\n\
"};

// Vertex shader for determining which pixels have a zero alpha, GLSL 1.50
static const char *GeometryZeroDstAlphaPixelMaskVtxShader_150 = {"\
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

// Fragment shader for determining which pixels have a zero alpha, GLSL 1.50
static const char *GeometryZeroDstAlphaPixelMaskFragShader_150 = {"\
in vec2 texCoord;\n\
uniform sampler2D texInFragColor;\n\
\n\
void main()\n\
{\n\
	vec4 inFragColor = texture(texInFragColor, texCoord);\n\
	\n\
	if (inFragColor.a <= 0.001)\n\
	{\n\
		discard;\n\
	}\n\
}\n\
"};

// Vertex shader for determining which pixels have a zero alpha, GLSL 1.50
static const char *MSGeometryZeroDstAlphaPixelMaskVtxShader_150 = {"\
in vec2 inPosition;\n\
in vec2 inTexCoord0;\n\
uniform sampler2DMS texInFragColor;\n\
out vec2 pixelCoord;\n\
\n\
void main()\n\
{\n\
	pixelCoord = inTexCoord0 * vec2(FRAMEBUFFER_SIZE_X, FRAMEBUFFER_SIZE_Y);\n\
	gl_Position = vec4(inPosition, 0.0, 1.0);\n\
}\n\
"};

// Fragment shader for determining which pixels have a zero alpha, GLSL 1.50
static const char *MSGeometryZeroDstAlphaPixelMaskFragShader_150 = {"\
in vec2 pixelCoord;\n\
uniform sampler2DMS texInFragColor;\n\
\n\
void main()\n\
{\n\
	vec4 inFragColor = texelFetch(texInFragColor, ivec2(pixelCoord), gl_SampleID);\n\
	\n\
	if (inFragColor.a <= 0.001)\n\
	{\n\
		discard;\n\
	}\n\
}\n\
"};

// Vertex shader for applying edge marking, GLSL 1.50
static const char *EdgeMarkVtxShader_150 = {"\
in vec2 inPosition;\n\
in vec2 inTexCoord0;\n\
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
	float fogDensity[32];\n\
	vec4 edgeColor[8];\n\
	vec4 toonColor[32];\n\
} state;\n\
\n\
out vec2 texCoord[5];\n\
out vec2 pixelCoord;\n\
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
	pixelCoord = inTexCoord0 * vec2(FRAMEBUFFER_SIZE_X, FRAMEBUFFER_SIZE_Y);\n\
	\n\
	gl_Position = vec4(inPosition, 0.0, 1.0);\n\
}\n\
"};

// Fragment shader for applying edge marking, GLSL 1.50
static const char *EdgeMarkFragShader_150 = {"\
in vec2 texCoord[5];\n\
in vec2 pixelCoord;\n\
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
	float fogDensity[32];\n\
	vec4 edgeColor[8];\n\
	vec4 toonColor[32];\n\
} state;\n\
\n\
uniform sampler2D texInFragDepth;\n\
uniform sampler2D texInPolyID;\n\
\n\
out vec4 outEdgeColor;\n\
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
	bool isWireframe[5];\n\
	isWireframe[0] = bool(polyIDInfo[0].g);\n\
	\n\
	float depth[5];\n\
	depth[0] = texture(texInFragDepth, texCoord[0]).r;\n\
	depth[1] = texture(texInFragDepth, texCoord[1]).r;\n\
	depth[2] = texture(texInFragDepth, texCoord[2]).r;\n\
	depth[3] = texture(texInFragDepth, texCoord[3]).r;\n\
	depth[4] = texture(texInFragDepth, texCoord[4]).r;\n\
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
		if ( ((pixelCoord.x >= FRAMEBUFFER_SIZE_X-1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[1]) && (depth[0] >= depth[1]) && !isWireframe[1])) )\n\
		{\n\
			if (pixelCoord.x >= FRAMEBUFFER_SIZE_X-1.0)\n\
			{\n\
				outEdgeColor = state.edgeColor[polyID[0]/8];\n\
			}\n\
			else\n\
			{\n\
				outEdgeColor = state.edgeColor[polyID[1]/8];\n\
			}\n\
		}\n\
		else if ( ((pixelCoord.y >= FRAMEBUFFER_SIZE_Y-1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[2]) && (depth[0] >= depth[2]) && !isWireframe[2])) )\n\
		{\n\
			if (pixelCoord.y >= FRAMEBUFFER_SIZE_Y-1.0)\n\
			{\n\
				outEdgeColor = state.edgeColor[polyID[0]/8];\n\
			}\n\
			else\n\
			{\n\
				outEdgeColor = state.edgeColor[polyID[2]/8];\n\
			}\n\
		}\n\
		else if ( ((pixelCoord.x < 1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[3]) && (depth[0] >= depth[3]) && !isWireframe[3])) )\n\
		{\n\
			if (pixelCoord.x < 1.0)\n\
			{\n\
				outEdgeColor = state.edgeColor[polyID[0]/8];\n\
			}\n\
			else\n\
			{\n\
				outEdgeColor = state.edgeColor[polyID[3]/8];\n\
			}\n\
		}\n\
		else if ( ((pixelCoord.y < 1.0) ? isEdgeMarkingClearValues : ((polyID[0] != polyID[4]) && (depth[0] >= depth[4]) && !isWireframe[4])) )\n\
		{\n\
			if (pixelCoord.y < 1.0)\n\
			{\n\
				outEdgeColor = state.edgeColor[polyID[0]/8];\n\
			}\n\
			else\n\
			{\n\
				outEdgeColor = state.edgeColor[polyID[4]/8];\n\
			}\n\
		}\n\
	}\n\
}\n\
"};

// Vertex shader for applying fog, GLSL 1.50
static const char *FogVtxShader_150 = {"\
in vec2 inPosition;\n\
in vec2 inTexCoord0;\n\
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
	float fogDensity[32];\n\
	vec4 edgeColor[8];\n\
	vec4 toonColor[32];\n\
} state;\n\
\n\
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
in vec2 texCoord;\n\
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
void main()\n\
{\n\
	outFragColor = texture(texInFragColor, texCoord);\n\
	\n\
	vec4 inFogAttributes = texture(texInFogAttributes, texCoord);\n\
	bool polyEnableFog = (inFogAttributes.r > 0.999);\n\
	\n\
	if (polyEnableFog)\n\
	{\n\
		float inFragDepth = texture(texInFragDepth, texCoord).r;\n\
		float fogMixWeight = 0.0;\n\
		\n\
		if (inFragDepth <= FOG_DEPTH_COMPARE_0)\n\
		{\n\
			fogMixWeight = state.fogDensity[0];\n\
		}\n\
		else if (inFragDepth >= FOG_DEPTH_COMPARE_31)\n\
		{\n\
			fogMixWeight = state.fogDensity[31];\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_1)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[ 0], state.fogDensity[ 1], (inFragDepth - FOG_DEPTH_COMPARE_0)  * FOG_DEPTH_INVDIFF_1);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_2)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[ 1], state.fogDensity[ 2], (inFragDepth - FOG_DEPTH_COMPARE_1)  * FOG_DEPTH_INVDIFF_2);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_3)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[ 2], state.fogDensity[ 3], (inFragDepth - FOG_DEPTH_COMPARE_2)  * FOG_DEPTH_INVDIFF_3);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_4)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[ 3], state.fogDensity[ 4], (inFragDepth - FOG_DEPTH_COMPARE_3)  * FOG_DEPTH_INVDIFF_4);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_5)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[ 4], state.fogDensity[ 5], (inFragDepth - FOG_DEPTH_COMPARE_4)  * FOG_DEPTH_INVDIFF_5);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_6)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[ 5], state.fogDensity[ 6], (inFragDepth - FOG_DEPTH_COMPARE_5)  * FOG_DEPTH_INVDIFF_6);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_7)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[ 6], state.fogDensity[ 7], (inFragDepth - FOG_DEPTH_COMPARE_6)  * FOG_DEPTH_INVDIFF_7);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_8)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[ 7], state.fogDensity[ 8], (inFragDepth - FOG_DEPTH_COMPARE_7)  * FOG_DEPTH_INVDIFF_8);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_9)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[ 8], state.fogDensity[ 9], (inFragDepth - FOG_DEPTH_COMPARE_8)  * FOG_DEPTH_INVDIFF_9);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_10)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[ 9], state.fogDensity[10], (inFragDepth - FOG_DEPTH_COMPARE_9)  * FOG_DEPTH_INVDIFF_10);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_11)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[10], state.fogDensity[11], (inFragDepth - FOG_DEPTH_COMPARE_10) * FOG_DEPTH_INVDIFF_11);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_12)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[11], state.fogDensity[12], (inFragDepth - FOG_DEPTH_COMPARE_11) * FOG_DEPTH_INVDIFF_12);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_13)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[12], state.fogDensity[13], (inFragDepth - FOG_DEPTH_COMPARE_12) * FOG_DEPTH_INVDIFF_13);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_14)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[13], state.fogDensity[14], (inFragDepth - FOG_DEPTH_COMPARE_13) * FOG_DEPTH_INVDIFF_14);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_15)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[14], state.fogDensity[15], (inFragDepth - FOG_DEPTH_COMPARE_14) * FOG_DEPTH_INVDIFF_15);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_16)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[15], state.fogDensity[16], (inFragDepth - FOG_DEPTH_COMPARE_15) * FOG_DEPTH_INVDIFF_16);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_17)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[16], state.fogDensity[17], (inFragDepth - FOG_DEPTH_COMPARE_16) * FOG_DEPTH_INVDIFF_17);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_18)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[17], state.fogDensity[18], (inFragDepth - FOG_DEPTH_COMPARE_17) * FOG_DEPTH_INVDIFF_18);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_19)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[18], state.fogDensity[19], (inFragDepth - FOG_DEPTH_COMPARE_18) * FOG_DEPTH_INVDIFF_19);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_20)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[19], state.fogDensity[20], (inFragDepth - FOG_DEPTH_COMPARE_19) * FOG_DEPTH_INVDIFF_20);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_21)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[20], state.fogDensity[21], (inFragDepth - FOG_DEPTH_COMPARE_20) * FOG_DEPTH_INVDIFF_21);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_22)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[21], state.fogDensity[22], (inFragDepth - FOG_DEPTH_COMPARE_21) * FOG_DEPTH_INVDIFF_22);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_23)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[22], state.fogDensity[23], (inFragDepth - FOG_DEPTH_COMPARE_22) * FOG_DEPTH_INVDIFF_23);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_24)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[23], state.fogDensity[24], (inFragDepth - FOG_DEPTH_COMPARE_23) * FOG_DEPTH_INVDIFF_24);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_25)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[24], state.fogDensity[25], (inFragDepth - FOG_DEPTH_COMPARE_24) * FOG_DEPTH_INVDIFF_25);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_26)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[25], state.fogDensity[26], (inFragDepth - FOG_DEPTH_COMPARE_25) * FOG_DEPTH_INVDIFF_26);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_27)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[26], state.fogDensity[27], (inFragDepth - FOG_DEPTH_COMPARE_26) * FOG_DEPTH_INVDIFF_27);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_28)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[27], state.fogDensity[28], (inFragDepth - FOG_DEPTH_COMPARE_27) * FOG_DEPTH_INVDIFF_28);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_29)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[28], state.fogDensity[29], (inFragDepth - FOG_DEPTH_COMPARE_28) * FOG_DEPTH_INVDIFF_29);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_30)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[29], state.fogDensity[30], (inFragDepth - FOG_DEPTH_COMPARE_29) * FOG_DEPTH_INVDIFF_30);\n\
		}\n\
		else if (inFragDepth <= FOG_DEPTH_COMPARE_31)\n\
		{\n\
			fogMixWeight = mix(state.fogDensity[30], state.fogDensity[31], (inFragDepth - FOG_DEPTH_COMPARE_30) * FOG_DEPTH_INVDIFF_31);\n\
		}\n\
		\n\
		outFragColor = mix(outFragColor, (state.enableFogAlphaOnly) ? vec4(outFragColor.rgb, state.fogColor.a) : state.fogColor, fogMixWeight);\n\
	}\n\
}\n\
"};

// Vertex shader for the final framebuffer, GLSL 1.50
static const char *FramebufferOutputVtxShader_150 = {"\
in vec2 inPosition;\n\
in vec2 inTexCoord0;\n\
\n\
out vec2 texCoord;\n\
\n\
void main()\n\
{\n\
	texCoord = vec2(inTexCoord0.x, (FRAMEBUFFER_SIZE_Y - (FRAMEBUFFER_SIZE_Y * inTexCoord0.y)) / FRAMEBUFFER_SIZE_Y);\n\
	gl_Position = vec4(inPosition, 0.0, 1.0);\n\
}\n\
"};

// Fragment shader for the final framebuffer, GLSL 1.50
static const char *FramebufferOutput6665FragShader_150 = {"\
in vec2 texCoord;\n\
\n\
uniform sampler2D texInFragColor;\n\
\n\
out vec4 outFragColor6665;\n\
\n\
void main()\n\
{\n\
	// Note that we swap B and R since pixel readbacks are done in BGRA format for fastest\n\
	// performance. The final color is still in RGBA format.\n\
	outFragColor6665     = texture(texInFragColor, texCoord).bgra;\n\
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
	_is64kUBOSupported = false;
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
	
	GLfloat maxAnisotropyOGL = 1.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropyOGL);
	this->_deviceInfo.maxAnisotropy = (float)maxAnisotropyOGL;
	
	this->_deviceInfo.isEdgeMarkSupported = true;
	this->_deviceInfo.isFogSupported = true;
	
	glGenTextures(1, &OGLRef.texFinalColorID);
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FinalColor);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texFinalColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	glActiveTexture(GL_TEXTURE0);
	
	// Load and create shaders. Return on any error, since v3.2 Core Profile makes shaders mandatory.
	this->isShaderSupported	= true;
	
	// OpenGL v3.2 Core Profile should have all the necessary features to be able to flip and convert the framebuffer.
	this->willFlipOnlyFramebufferOnGPU = true;
	this->willFlipAndConvertFramebufferOnGPU = true;
	
	this->isSampleShadingSupported = this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_sample_shading");
	this->isConservativeDepthSupported = this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_conservative_depth") && IsOpenGLDriverVersionSupported(4, 0, 0);
	this->isConservativeDepthAMDSupported = this->IsExtensionPresent(&oglExtensionSet, "GL_AMD_conservative_depth") && IsOpenGLDriverVersionSupported(4, 0, 0);
	
	this->_enableTextureSmoothing = CommonSettings.GFX3D_Renderer_TextureSmoothing;
	this->_emulateShadowPolygon = CommonSettings.OpenGL_Emulation_ShadowPolygon;
	this->_emulateSpecialZeroAlphaBlending = CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending;
	this->_emulateNDSDepthCalculation = CommonSettings.OpenGL_Emulation_NDSDepthCalculation;
	this->_emulateDepthLEqualPolygonFacing = CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing;
	
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
	
	if (this->isSampleShadingSupported)
	{
		error = this->CreateMSGeometryZeroDstAlphaProgram(MSGeometryZeroDstAlphaPixelMaskVtxShader_150, MSGeometryZeroDstAlphaPixelMaskFragShader_150);
		if (error == OGLERROR_NOERR)
		{
			this->willUsePerSampleZeroDstPass = true;
		}
		else
		{
			glUseProgram(0);
			this->DestroyGeometryPrograms();
			this->DestroyGeometryZeroDstAlphaProgram();
			this->isShaderSupported = false;
			this->isSampleShadingSupported = false;
			this->willUsePerSampleZeroDstPass = false;
			
			return error;
		}
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
	
	this->_enableMultisampledRendering = ((this->_selectedMultisampleSize >= 2) && this->isMultisampledFBOSupported);
	
	this->InitFinalRenderStates(&oglExtensionSet); // This must be done last
	
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, this->_framebufferWidth, this->_framebufferHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GColor);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGColorID);
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
	
	CACHE_ALIGN GLint tempClearImageBuffer[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	memset(tempClearImageBuffer, 0, sizeof(tempClearImageBuffer));
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texCIColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, tempClearImageBuffer);
	
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, tempClearImageBuffer);
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	// Set up FBOs
	glGenFramebuffers(1, &OGLRef.fboClearImageID);
	glGenFramebuffers(1, &OGLRef.fboRenderID);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboClearImageID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, OGLRef.texCIColorID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, OGLRef.texCIFogAttrID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, OGLRef.texCIDepthStencilID, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to create FBOs!\n");
		this->DestroyFBOs();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboRenderID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, OGLRef.texGColorID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, OGLRef.texGPolyID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, OGLRef.texGFogAttrID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, OGLRef.texFinalColorID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, OGLRef.texGDepthStencilID, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to create FBOs!\n");
		this->DestroyFBOs();
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	OGLRef.selectedRenderingFBO = OGLRef.fboRenderID;
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
	glDeleteTextures(1, &OGLRef.texCIColorID);
	glDeleteTextures(1, &OGLRef.texCIFogAttrID);
	glDeleteTextures(1, &OGLRef.texCIDepthStencilID);
	glDeleteTextures(1, &OGLRef.texGColorID);
	glDeleteTextures(1, &OGLRef.texGPolyID);
	glDeleteTextures(1, &OGLRef.texGFogAttrID);
	glDeleteTextures(1, &OGLRef.texGDepthStencilID);
	
	OGLRef.fboClearImageID = 0;
	OGLRef.fboRenderID = 0;
	
	this->isFBOSupported = false;
}

Render3DError OpenGLRenderer_3_2::CreateMultisampledFBO(GLsizei numSamples)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up FBO render targets
	glGenRenderbuffers(1, &OGLRef.rboMSGPolyID);
	glGenRenderbuffers(1, &OGLRef.rboMSGFogAttrID);
	glGenRenderbuffers(1, &OGLRef.rboMSGDepthStencilID);
	
	if (this->willUsePerSampleZeroDstPass)
	{
		glGenTextures(1, &OGLRef.texMSGColorID);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGColorID);
		glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight, GL_TRUE);
	}
	else
	{
		glGenRenderbuffers(1, &OGLRef.rboMSGColorID);
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGColorID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight);
	}
	
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGPolyID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGFogAttrID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_RGBA, this->_framebufferWidth, this->_framebufferHeight);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGDepthStencilID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_DEPTH24_STENCIL8, this->_framebufferWidth, this->_framebufferHeight);
	
	// Set up multisampled rendering FBO
	glGenFramebuffers(1, &OGLRef.fboMSIntermediateRenderID);
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboMSIntermediateRenderID);
	
	if (this->willUsePerSampleZeroDstPass)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGColorID, 0);
	}
	else
	{
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, OGLRef.rboMSGColorID);
	}
	
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, OGLRef.rboMSGPolyID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_RENDERBUFFER, OGLRef.rboMSGFogAttrID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, OGLRef.rboMSGDepthStencilID);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to create multisampled FBO. Multisample antialiasing will be disabled.\n");
		this->DestroyMultisampledFBO();
		
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
	glDeleteTextures(1, &OGLRef.texMSGColorID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGColorID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGPolyID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGFogAttrID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGDepthStencilID);
	
	OGLRef.fboMSIntermediateRenderID = 0;
	
	this->isMultisampledFBOSupported = false;
}

void OpenGLRenderer_3_2::ResizeMultisampledFBOs(GLsizei numSamples)
{
	OGLRenderRef &OGLRef = *this->ref;
	GLsizei w = this->_framebufferWidth;
	GLsizei h = this->_framebufferHeight;
	
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
	
	if (this->willUsePerSampleZeroDstPass)
	{
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, OGLRef.texMSGColorID);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_RGBA, w, h, GL_TRUE);
	}
	else
	{
		glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGColorID);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_RGBA, w, h);
	}
	
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGPolyID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_RGBA, w, h);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGFogAttrID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_RGBA, w, h);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGDepthStencilID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_DEPTH24_STENCIL8, w, h);
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
	glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
	
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
		if (OGLRef.uboPolyStatesID == 0)
		{
			glGenBuffers(1, &OGLRef.uboPolyStatesID);
			glBindBuffer(GL_UNIFORM_BUFFER, OGLRef.uboPolyStatesID);
			glBufferData(GL_UNIFORM_BUFFER, MAX_CLIPPED_POLY_COUNT_FOR_UBO * sizeof(OGLPolyStates), NULL, GL_DYNAMIC_DRAW);
			glBindBufferBase(GL_UNIFORM_BUFFER, OGLBindingPointID_PolyStates, OGLRef.uboPolyStatesID);
		}
	}
	else
	{
		if (OGLRef.tboPolyStatesID == 0)
		{
			// Set up poly states TBO
			glGenBuffers(1, &OGLRef.tboPolyStatesID);
			glBindBuffer(GL_TEXTURE_BUFFER, OGLRef.tboPolyStatesID);
			glBufferData(GL_TEXTURE_BUFFER, POLYLIST_SIZE * sizeof(OGLPolyStates), NULL, GL_DYNAMIC_DRAW);
			
			glGenTextures(1, &OGLRef.texPolyStatesID);
			glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_PolyStates);
			glBindTexture(GL_TEXTURE_BUFFER, OGLRef.texPolyStatesID);
			glTexBuffer(GL_TEXTURE_BUFFER, GL_R32I, OGLRef.tboPolyStatesID);
			glActiveTexture(GL_TEXTURE0);
		}
	}
	
	OGLGeometryFlags programFlags;
	programFlags.value = 0;
	
	std::stringstream vtxShaderHeader;
	if (this->isConservativeDepthSupported || this->isConservativeDepthAMDSupported)
	{
		vtxShaderHeader << "#version 400\n";
	}
	else
	{
		vtxShaderHeader << "#version 150\n";
	}
	vtxShaderHeader << "\n";
	vtxShaderHeader << "#define IS_USING_UBO_POLY_STATES " << ((OGLRef.uboPolyStatesID != 0) ? 1 : 0) << "\n";
	vtxShaderHeader << "\n";
	
	std::string vtxShaderCode  = vtxShaderHeader.str() + std::string(GeometryVtxShader_150);
	
	std::stringstream fragShaderHeader;
	if (this->isConservativeDepthSupported || this->isConservativeDepthAMDSupported)
	{
		fragShaderHeader << "#version 400\n";
		
		// Prioritize using GL_AMD_conservative_depth over GL_ARB_conservative_depth, since AMD drivers
		// seem to have problems with GL_ARB_conservative_depth.
		fragShaderHeader << ((this->isConservativeDepthAMDSupported) ? "#extension GL_AMD_conservative_depth : require\n" : "#extension GL_ARB_conservative_depth : require\n");
	}
	else
	{
		fragShaderHeader << "#version 150\n";
	}
	fragShaderHeader << "\n";
	fragShaderHeader << "#define IS_CONSERVATIVE_DEPTH_SUPPORTED " << ((this->isConservativeDepthSupported || this->isConservativeDepthAMDSupported) ? 1 : 0) << "\n";
	fragShaderHeader << "#define DEPTH_EQUALS_TEST_TOLERANCE " << DEPTH_EQUALS_TEST_TOLERANCE << ".0\n";
	fragShaderHeader << "\n";
	
	for (size_t flagsValue = 0; flagsValue < 128; flagsValue++, programFlags.value++)
	{
		std::stringstream shaderFlags;
		shaderFlags << "#define USE_TEXTURE_SMOOTHING " << ((this->_enableTextureSmoothing) ? 1 : 0) << "\n";
		shaderFlags << "#define USE_NDS_DEPTH_CALCULATION " << ((this->_emulateNDSDepthCalculation) ? 1 : 0) << "\n";
		shaderFlags << "\n";
		shaderFlags << "#define ENABLE_W_DEPTH " << ((programFlags.EnableWDepth) ? 1 : 0) << "\n";
		shaderFlags << "#define ENABLE_ALPHA_TEST " << ((programFlags.EnableAlphaTest) ? "true\n" : "false\n");
		shaderFlags << "#define ENABLE_TEXTURE_SAMPLING " << ((programFlags.EnableTextureSampling) ? "true\n" : "false\n");
		shaderFlags << "#define ENABLE_FOG " << ((programFlags.EnableFog) ? 1 : 0) << "\n";
		shaderFlags << "#define ENABLE_EDGE_MARK " << ((programFlags.EnableEdgeMark) ? 1 : 0) << "\n";
		shaderFlags << "#define TOON_SHADING_MODE " << ((programFlags.ToonShadingMode) ? 1 : 0) << "\n";
		shaderFlags << "#define NEEDS_DEPTH_EQUALS_TEST " << ((programFlags.NeedsDepthEqualsTest) ? 1 : 0) << "\n";
		shaderFlags << "\n";
		
		std::string fragShaderCode = fragShaderHeader.str() + shaderFlags.str() + std::string(GeometryFragShader_150);
		
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
		glBindFragDataLocation(OGLRef.programGeometryID[flagsValue], 0, "outFragColor");
		
		switch (programFlags.DrawBuffersMode)
		{
			case OGLGeometryDrawBuffersMode_Color_Fog:
				glBindFragDataLocation(OGLRef.programGeometryID[flagsValue], 1, "outFogAttributes");
				break;
				
			case OGLGeometryDrawBuffersMode_Color_EdgeMark:
				glBindFragDataLocation(OGLRef.programGeometryID[flagsValue], 1, "outPolyID");
				break;
				
			case OGLGeometryDrawBuffersMode_Color_Fog_EdgeMark:
				glBindFragDataLocation(OGLRef.programGeometryID[flagsValue], 1, "outPolyID");
				glBindFragDataLocation(OGLRef.programGeometryID[flagsValue], 2, "outFogAttributes");
				break;
				
			default:
				break;
		}
		
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
			const GLuint uniformBlockPolyStates = glGetUniformBlockIndex(OGLRef.programGeometryID[flagsValue], "PolyStates");
			glUniformBlockBinding(OGLRef.programGeometryID[flagsValue], uniformBlockPolyStates, OGLBindingPointID_PolyStates);
		}
		else
		{
			const GLint uniformTexBufferPolyStates = glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "PolyStates");
			glUniform1i(uniformTexBufferPolyStates, OGLTextureUnitID_PolyStates);
		}
		
		OGLRef.uniformTexDrawOpaque[flagsValue]			= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "texDrawOpaque");
		OGLRef.uniformPolyDrawShadow[flagsValue]		= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyDrawShadow");
		OGLRef.uniformPolyStateIndex[flagsValue]		= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyIndex");
		OGLRef.uniformPolyDepthOffsetMode[flagsValue]	= glGetUniformLocation(OGLRef.programGeometryID[flagsValue], "polyDepthOffsetMode");
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
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	glDeleteBuffers(1, &OGLRef.uboRenderStatesID);
	glDeleteBuffers(1, &OGLRef.uboPolyStatesID);
	glDeleteBuffers(1, &OGLRef.tboPolyStatesID);
	
	OGLRef.uboRenderStatesID = 0;
	OGLRef.uboPolyStatesID = 0;
	OGLRef.tboPolyStatesID = 0;
	
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
	shaderHeader << "#version 150\n";
	shaderHeader << "\n";
	
	std::string vtxShaderCode  = shaderHeader.str() + std::string(vtxShaderCString);
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

Render3DError OpenGLRenderer_3_2::CreateMSGeometryZeroDstAlphaProgram(const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (vtxShaderCString == NULL) || (fragShaderCString == NULL) )
	{
		return error;
	}
	
	std::stringstream shaderHeader;
	shaderHeader << "#version 150\n";
	shaderHeader << "#extension GL_ARB_sample_shading : require\n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_X " << this->_framebufferWidth  << ".0 \n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_Y " << this->_framebufferHeight << ".0 \n";
	shaderHeader << "\n";
	
	std::string vtxShaderCode  = shaderHeader.str() + std::string(vtxShaderCString);
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
	
	glBindAttribLocation(OGLRef.programMSGeometryZeroDstAlphaID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programMSGeometryZeroDstAlphaID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	
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
	glUniform1i(uniformTexGColor, 0);
	
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

Render3DError OpenGLRenderer_3_2::CreateEdgeMarkProgram(const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (vtxShaderCString == NULL) || (fragShaderCString == NULL) )
	{
		return error;
	}
	
	std::stringstream shaderHeader;
	shaderHeader << "#version 150\n";
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
	glBindFragDataLocation(OGLRef.programEdgeMarkID, 0, "outEdgeColor");
	
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
	
	const GLuint uniformBlockRenderStates = glGetUniformBlockIndex(OGLRef.programEdgeMarkID, "RenderStates");
	glUniformBlockBinding(OGLRef.programEdgeMarkID, uniformBlockRenderStates, OGLBindingPointID_RenderStates);
	
	const GLint uniformTexGDepth  = glGetUniformLocation(OGLRef.programEdgeMarkID, "texInFragDepth");
	const GLint uniformTexGPolyID = glGetUniformLocation(OGLRef.programEdgeMarkID, "texInPolyID");
	glUniform1i(uniformTexGDepth, OGLTextureUnitID_DepthStencil);
	glUniform1i(uniformTexGPolyID, OGLTextureUnitID_GPolyID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::CreateFogProgram(const OGLFogProgramKey fogProgramKey, const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (vtxShaderCString == NULL) || (fragShaderCString == NULL) )
	{
		return error;
	}
	
	const u16 fogOffset = fogProgramKey.offset;
	const u16 fogShift = (0x0400 >> fogProgramKey.shift);
	
	const GLfloat fogDepthCompare[32] = {
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift *  1)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift *  2)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift *  3)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift *  4)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift *  5)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift *  6)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift *  7)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift *  8)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift *  9)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 10)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 11)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 12)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 13)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 14)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 15)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 16)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 17)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 18)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 19)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 20)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 21)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 22)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 23)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 24)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 25)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 26)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 27)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 28)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 29)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 30)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 31)) / 32767.0f, 1.0f),
		std::min<GLfloat>((GLfloat)(fogOffset + (fogShift * 32)) / 32767.0f, 1.0f)
	};
	
	std::stringstream shaderHeader;
	shaderHeader << "#version 150\n";
	shaderHeader << "\n";
	
	std::stringstream fragDepthConstants;
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_0  " << fogDepthCompare[ 0] << (((fogDepthCompare[ 0] == 0.0f) || (fogDepthCompare[ 0] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_1  " << fogDepthCompare[ 1] << (((fogDepthCompare[ 1] == 0.0f) || (fogDepthCompare[ 1] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_2  " << fogDepthCompare[ 2] << (((fogDepthCompare[ 2] == 0.0f) || (fogDepthCompare[ 2] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_3  " << fogDepthCompare[ 3] << (((fogDepthCompare[ 3] == 0.0f) || (fogDepthCompare[ 3] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_4  " << fogDepthCompare[ 4] << (((fogDepthCompare[ 4] == 0.0f) || (fogDepthCompare[ 4] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_5  " << fogDepthCompare[ 5] << (((fogDepthCompare[ 5] == 0.0f) || (fogDepthCompare[ 5] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_6  " << fogDepthCompare[ 6] << (((fogDepthCompare[ 6] == 0.0f) || (fogDepthCompare[ 6] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_7  " << fogDepthCompare[ 7] << (((fogDepthCompare[ 7] == 0.0f) || (fogDepthCompare[ 7] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_8  " << fogDepthCompare[ 8] << (((fogDepthCompare[ 8] == 0.0f) || (fogDepthCompare[ 8] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_9  " << fogDepthCompare[ 9] << (((fogDepthCompare[ 9] == 0.0f) || (fogDepthCompare[ 9] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_10 " << fogDepthCompare[10] << (((fogDepthCompare[10] == 0.0f) || (fogDepthCompare[10] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_11 " << fogDepthCompare[11] << (((fogDepthCompare[11] == 0.0f) || (fogDepthCompare[11] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_12 " << fogDepthCompare[12] << (((fogDepthCompare[12] == 0.0f) || (fogDepthCompare[12] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_13 " << fogDepthCompare[13] << (((fogDepthCompare[13] == 0.0f) || (fogDepthCompare[13] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_14 " << fogDepthCompare[14] << (((fogDepthCompare[14] == 0.0f) || (fogDepthCompare[14] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_15 " << fogDepthCompare[15] << (((fogDepthCompare[15] == 0.0f) || (fogDepthCompare[15] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_16 " << fogDepthCompare[16] << (((fogDepthCompare[16] == 0.0f) || (fogDepthCompare[16] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_17 " << fogDepthCompare[17] << (((fogDepthCompare[17] == 0.0f) || (fogDepthCompare[17] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_18 " << fogDepthCompare[18] << (((fogDepthCompare[18] == 0.0f) || (fogDepthCompare[18] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_19 " << fogDepthCompare[19] << (((fogDepthCompare[19] == 0.0f) || (fogDepthCompare[19] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_20 " << fogDepthCompare[20] << (((fogDepthCompare[20] == 0.0f) || (fogDepthCompare[20] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_21 " << fogDepthCompare[21] << (((fogDepthCompare[21] == 0.0f) || (fogDepthCompare[21] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_22 " << fogDepthCompare[22] << (((fogDepthCompare[22] == 0.0f) || (fogDepthCompare[22] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_23 " << fogDepthCompare[23] << (((fogDepthCompare[23] == 0.0f) || (fogDepthCompare[23] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_24 " << fogDepthCompare[24] << (((fogDepthCompare[24] == 0.0f) || (fogDepthCompare[24] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_25 " << fogDepthCompare[25] << (((fogDepthCompare[25] == 0.0f) || (fogDepthCompare[25] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_26 " << fogDepthCompare[26] << (((fogDepthCompare[26] == 0.0f) || (fogDepthCompare[26] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_27 " << fogDepthCompare[27] << (((fogDepthCompare[27] == 0.0f) || (fogDepthCompare[27] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_28 " << fogDepthCompare[28] << (((fogDepthCompare[28] == 0.0f) || (fogDepthCompare[28] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_29 " << fogDepthCompare[29] << (((fogDepthCompare[29] == 0.0f) || (fogDepthCompare[29] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_30 " << fogDepthCompare[30] << (((fogDepthCompare[30] == 0.0f) || (fogDepthCompare[30] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "#define FOG_DEPTH_COMPARE_31 " << fogDepthCompare[31] << (((fogDepthCompare[31] == 0.0f) || (fogDepthCompare[31] == 1.0f)) ? ".0" : "") << "\n";
	fragDepthConstants << "\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_0   0.0\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_1  (1.0 / (FOG_DEPTH_COMPARE_1  - FOG_DEPTH_COMPARE_0))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_2  (1.0 / (FOG_DEPTH_COMPARE_2  - FOG_DEPTH_COMPARE_1))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_3  (1.0 / (FOG_DEPTH_COMPARE_3  - FOG_DEPTH_COMPARE_2))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_4  (1.0 / (FOG_DEPTH_COMPARE_4  - FOG_DEPTH_COMPARE_3))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_5  (1.0 / (FOG_DEPTH_COMPARE_5  - FOG_DEPTH_COMPARE_4))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_6  (1.0 / (FOG_DEPTH_COMPARE_6  - FOG_DEPTH_COMPARE_5))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_7  (1.0 / (FOG_DEPTH_COMPARE_7  - FOG_DEPTH_COMPARE_6))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_8  (1.0 / (FOG_DEPTH_COMPARE_8  - FOG_DEPTH_COMPARE_7))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_9  (1.0 / (FOG_DEPTH_COMPARE_9  - FOG_DEPTH_COMPARE_8))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_10 (1.0 / (FOG_DEPTH_COMPARE_10 - FOG_DEPTH_COMPARE_9))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_11 (1.0 / (FOG_DEPTH_COMPARE_11 - FOG_DEPTH_COMPARE_10))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_12 (1.0 / (FOG_DEPTH_COMPARE_12 - FOG_DEPTH_COMPARE_11))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_13 (1.0 / (FOG_DEPTH_COMPARE_13 - FOG_DEPTH_COMPARE_12))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_14 (1.0 / (FOG_DEPTH_COMPARE_14 - FOG_DEPTH_COMPARE_13))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_15 (1.0 / (FOG_DEPTH_COMPARE_15 - FOG_DEPTH_COMPARE_14))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_16 (1.0 / (FOG_DEPTH_COMPARE_16 - FOG_DEPTH_COMPARE_15))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_17 (1.0 / (FOG_DEPTH_COMPARE_17 - FOG_DEPTH_COMPARE_16))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_18 (1.0 / (FOG_DEPTH_COMPARE_18 - FOG_DEPTH_COMPARE_17))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_19 (1.0 / (FOG_DEPTH_COMPARE_19 - FOG_DEPTH_COMPARE_18))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_20 (1.0 / (FOG_DEPTH_COMPARE_20 - FOG_DEPTH_COMPARE_19))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_21 (1.0 / (FOG_DEPTH_COMPARE_21 - FOG_DEPTH_COMPARE_20))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_22 (1.0 / (FOG_DEPTH_COMPARE_22 - FOG_DEPTH_COMPARE_21))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_23 (1.0 / (FOG_DEPTH_COMPARE_23 - FOG_DEPTH_COMPARE_22))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_24 (1.0 / (FOG_DEPTH_COMPARE_24 - FOG_DEPTH_COMPARE_23))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_25 (1.0 / (FOG_DEPTH_COMPARE_25 - FOG_DEPTH_COMPARE_24))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_26 (1.0 / (FOG_DEPTH_COMPARE_26 - FOG_DEPTH_COMPARE_25))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_27 (1.0 / (FOG_DEPTH_COMPARE_27 - FOG_DEPTH_COMPARE_26))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_28 (1.0 / (FOG_DEPTH_COMPARE_28 - FOG_DEPTH_COMPARE_27))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_29 (1.0 / (FOG_DEPTH_COMPARE_29 - FOG_DEPTH_COMPARE_28))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_30 (1.0 / (FOG_DEPTH_COMPARE_30 - FOG_DEPTH_COMPARE_29))\n";
	fragDepthConstants << "#define FOG_DEPTH_INVDIFF_31 (1.0 / (FOG_DEPTH_COMPARE_31 - FOG_DEPTH_COMPARE_30))\n";
	fragDepthConstants << "\n";
	
	std::string vtxShaderCode  = shaderHeader.str() + std::string(vtxShaderCString);
	std::string fragShaderCode = shaderHeader.str() + fragDepthConstants.str() + std::string(fragShaderCString);
	
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
	
	glBindAttribLocation(shaderID.program, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(shaderID.program, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	glBindFragDataLocation(shaderID.program, 0, "outFragColor");
	
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
	
	const GLint uniformTexGColor	= glGetUniformLocation(shaderID.program, "texInFragColor");
	const GLint uniformTexGDepth	= glGetUniformLocation(shaderID.program, "texInFragDepth");
	const GLint uniformTexGFog		= glGetUniformLocation(shaderID.program, "texInFogAttributes");
	glUniform1i(uniformTexGColor, OGLTextureUnitID_GColor);
	glUniform1i(uniformTexGDepth, OGLTextureUnitID_DepthStencil);
	glUniform1i(uniformTexGFog, OGLTextureUnitID_FogAttr);
	
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
	shaderHeader << "#version 150\n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_X " << this->_framebufferWidth  << ".0 \n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_Y " << this->_framebufferHeight << ".0 \n";
	shaderHeader << "\n";
	
	std::string vtxShaderCode  = shaderHeader.str() + std::string(vtxShaderCString);
	std::string fragShaderCode = shaderHeader.str() + std::string(fragShaderCString);
	
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
	
	glBindAttribLocation(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex], OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex], OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	glBindFragDataLocation(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex], 0, "outFragColor6665");
	
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
	for (size_t i = 0; i < extensionCount; i++)
	{
		std::string extensionName = std::string((const char *)glGetStringi(GL_EXTENSIONS, i));
		oglExtensionSet->insert(extensionName);
	}
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

Render3DError OpenGLRenderer_3_2::ZeroDstAlphaPass(const POLYLIST *polyList, const INDEXLIST *indexList, bool enableAlphaBlending, size_t indexOffset, POLYGON_ATTR lastPolyAttr)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Pre Pass: Fill in the stencil buffer based on the alpha of the current framebuffer color.
	// Fully transparent pixels (alpha == 0) -- Set stencil buffer to 0
	// All other pixels (alpha != 0) -- Set stencil buffer to 1
	
	this->DisableVertexAttributes();
	
	const bool isRunningMSAA = this->isMultisampledFBOSupported && (OGLRef.selectedRenderingFBO == OGLRef.fboMSIntermediateRenderID);
	const bool isRunningMSAAWithPerSampleShading = isRunningMSAA && this->willUsePerSampleZeroDstPass; // Doing per-sample shading should be a little more accurate than not doing so.
	
	if (isRunningMSAA && !isRunningMSAAWithPerSampleShading)
	{
		// Just downsample the color buffer now so that we have some texture data to sample from in the non-multisample shader.
		// This is not perfectly pixel accurate, but it's better than nothing.
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.fboRenderID);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glDrawBuffers(3, GeometryDrawBuffersList[this->_geometryProgramFlags.DrawBuffersMode]);
		glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
	}
	
	glUseProgram((isRunningMSAAWithPerSampleShading) ? OGLRef.programMSGeometryZeroDstAlphaID : OGLRef.programGeometryZeroDstAlphaID);
	glViewport(0, 0, this->_framebufferWidth, this->_framebufferHeight);
	glDisable(GL_BLEND);
	glEnable(GL_STENCIL_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	
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
	
	glUseProgram(OGLRef.programGeometryID[this->_geometryProgramFlags.value]);
	glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_FALSE);
	glUniform1i(OGLRef.uniformPolyDrawShadow[this->_geometryProgramFlags.value], GL_FALSE);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
	this->EnableVertexAttributes();
	
	// Draw the alpha polys, touching fully transparent pixels only once.
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glEnable(GL_DEPTH_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	glStencilFunc(GL_NOTEQUAL, 0x40, 0x40);
	
	this->DrawPolygonsForIndexRange<OGLPolyDrawMode_ZeroAlphaPass>(polyList, indexList, this->_clippedPolyOpaqueCount, this->_clippedPolyCount - 1, indexOffset, lastPolyAttr);
	
	// Restore OpenGL states back to normal.
	this->_geometryProgramFlags = oldGProgramFlags;
	glUseProgram(OGLRef.programGeometryID[this->_geometryProgramFlags.value]);
	glDrawBuffers(3, GeometryDrawBuffersList[this->_geometryProgramFlags.DrawBuffersMode]);
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

Render3DError OpenGLRenderer_3_2::DownsampleFBO()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isMultisampledFBOSupported && (OGLRef.selectedRenderingFBO == OGLRef.fboMSIntermediateRenderID))
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.fboMSIntermediateRenderID);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.fboRenderID);
		
		if (this->_enableEdgeMark)
		{
			// Blit the polygon ID buffer
			glReadBuffer(GL_COLOR_ATTACHMENT1);
			glDrawBuffer(GL_COLOR_ATTACHMENT1);
			glBlitFramebuffer(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		
		if (this->_enableFog)
		{
			// Blit the fog buffer
			glReadBuffer(GL_COLOR_ATTACHMENT2);
			glDrawBuffer(GL_COLOR_ATTACHMENT2);
			glBlitFramebuffer(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		
		// Blit the color and depth buffers
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		
		// Reset framebuffer targets
		glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboRenderID);
		glDrawBuffers(3, GeometryDrawBuffersList[this->_geometryProgramFlags.DrawBuffersMode]);
	}
	
	return OGLERROR_NOERR;
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
			glDrawBuffer(GL_COLOR_ATTACHMENT3);
			glReadBuffer(GL_COLOR_ATTACHMENT3);
		}
		else
		{
			// Use the program where the output color is from index 0.
			glUseProgram(OGLRef.programFramebufferRGBA6665OutputID[0]);
			glDrawBuffer(GL_COLOR_ATTACHMENT0);
			glReadBuffer(GL_COLOR_ATTACHMENT0);
		}
		
		glViewport(0, 0, this->_framebufferWidth, this->_framebufferHeight);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		
		glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
		glBindVertexArray(OGLRef.vaoPostprocessStatesID);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(0);
		
		if (this->_mappedFramebuffer != NULL)
		{
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			this->_mappedFramebuffer = NULL;
		}
		
		glReadPixels(0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_BGRA, GL_UNSIGNED_BYTE, 0);
	}
	else
	{
		// Just flips the framebuffer in Y to match the coordinates of OpenGL and the NDS hardware.
		if (this->_lastTextureDrawTarget == OGLTextureUnitID_GColor)
		{
			glDrawBuffer(GL_COLOR_ATTACHMENT3);
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			glBlitFramebuffer(0, this->_framebufferHeight, this->_framebufferWidth, 0, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glReadBuffer(GL_COLOR_ATTACHMENT3);
		}
		else
		{
			glDrawBuffer(GL_COLOR_ATTACHMENT0);
			glReadBuffer(GL_COLOR_ATTACHMENT3);
			glBlitFramebuffer(0, this->_framebufferHeight, this->_framebufferWidth, 0, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glReadBuffer(GL_COLOR_ATTACHMENT0);
		}
		
		// Read back the pixels in RGBA format, since an OpenGL 3.2 device should be able to read back this
		// format without a performance penalty.
		if (this->_mappedFramebuffer != NULL)
		{
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			this->_mappedFramebuffer = NULL;
		}
		
		glReadPixels(0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	}
	
	this->_pixelReadNeedsFinish = true;
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::BeginRender(const GFX3D &engine)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!BEGINGL())
	{
		return OGLERROR_BEGINGL_FAILED;
	}
	
	this->_clippedPolyCount = engine.clippedPolyCount;
	this->_clippedPolyOpaqueCount = engine.clippedPolyOpaqueCount;
	this->_clippedPolyList = engine.clippedPolyList;
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
	
	// Copy the vertex data to the GPU asynchronously due to the potentially large upload size.
	// This buffer write will need to be synchronized before we start drawing.
	if (this->_syncBufferSetup != NULL)
	{
		glWaitSync(this->_syncBufferSetup, 0, GL_TIMEOUT_IGNORED);
		glDeleteSync(this->_syncBufferSetup);
	}
	
	const size_t vtxBufferSize = sizeof(VERT) * engine.vertListCount;
	VERT *vtxPtr = (VERT *)glMapBufferRange(GL_ARRAY_BUFFER, 0, vtxBufferSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
	memcpy(vtxPtr, engine.vertList, vtxBufferSize);
	glUnmapBuffer(GL_ARRAY_BUFFER);
	
	this->_syncBufferSetup = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	
	// Set up rendering states that will remain constant for the entire frame.
	this->_pendingRenderStates.enableAntialiasing = (engine.renderState.enableAntialiasing) ? GL_TRUE : GL_FALSE;
	this->_pendingRenderStates.enableFogAlphaOnly = (engine.renderState.enableFogAlphaOnly) ? GL_TRUE : GL_FALSE;
	this->_pendingRenderStates.clearPolyID = this->_clearAttributes.opaquePolyID;
	this->_pendingRenderStates.clearDepth = (GLfloat)this->_clearAttributes.depth / (GLfloat)0x00FFFFFF;
	this->_pendingRenderStates.alphaTestRef = divide5bitBy31_LUT[engine.renderState.alphaTestRef];
	this->_pendingRenderStates.fogColor.r = divide5bitBy31_LUT[(engine.renderState.fogColor      ) & 0x0000001F];
	this->_pendingRenderStates.fogColor.g = divide5bitBy31_LUT[(engine.renderState.fogColor >>  5) & 0x0000001F];
	this->_pendingRenderStates.fogColor.b = divide5bitBy31_LUT[(engine.renderState.fogColor >> 10) & 0x0000001F];
	this->_pendingRenderStates.fogColor.a = divide5bitBy31_LUT[(engine.renderState.fogColor >> 16) & 0x0000001F];
	this->_pendingRenderStates.fogOffset = (GLfloat)(engine.renderState.fogOffset & 0x7FFF) / 32767.0f;
	this->_pendingRenderStates.fogStep = (GLfloat)(0x0400 >> engine.renderState.fogShift) / 32767.0f;
	
	for (size_t i = 0; i < 32; i++)
	{
		this->_pendingRenderStates.fogDensity[i].r = (engine.renderState.fogDensityTable[i] == 127) ? 1.0f : (GLfloat)engine.renderState.fogDensityTable[i] / 128.0f;
		this->_pendingRenderStates.fogDensity[i].g = 0.0f;
		this->_pendingRenderStates.fogDensity[i].b = 0.0f;
		this->_pendingRenderStates.fogDensity[i].a = 0.0f;
	}
	
	const GLfloat edgeColorAlpha = (engine.renderState.enableAntialiasing) ? (16.0f/31.0f) : 1.0f;
	for (size_t i = 0; i < 8; i++)
	{
		this->_pendingRenderStates.edgeColor[i].r = divide5bitBy31_LUT[(engine.renderState.edgeMarkColorTable[i]      ) & 0x001F];
		this->_pendingRenderStates.edgeColor[i].g = divide5bitBy31_LUT[(engine.renderState.edgeMarkColorTable[i] >>  5) & 0x001F];
		this->_pendingRenderStates.edgeColor[i].b = divide5bitBy31_LUT[(engine.renderState.edgeMarkColorTable[i] >> 10) & 0x001F];
		this->_pendingRenderStates.edgeColor[i].a = edgeColorAlpha;
	}
	
	for (size_t i = 0; i < 32; i++)
	{
		this->_pendingRenderStates.toonColor[i].r = divide5bitBy31_LUT[(engine.renderState.u16ToonTable[i]      ) & 0x001F];
		this->_pendingRenderStates.toonColor[i].g = divide5bitBy31_LUT[(engine.renderState.u16ToonTable[i] >>  5) & 0x001F];
		this->_pendingRenderStates.toonColor[i].b = divide5bitBy31_LUT[(engine.renderState.u16ToonTable[i] >> 10) & 0x001F];
		this->_pendingRenderStates.toonColor[i].a = 1.0f;
	}
	
	glBindBuffer(GL_UNIFORM_BUFFER, OGLRef.uboRenderStatesID);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(this->_pendingRenderStates), &this->_pendingRenderStates);
	
	// Generate the clipped polygon list.
	if ( (OGLRef.uboPolyStatesID != 0) && (this->_clippedPolyCount > MAX_CLIPPED_POLY_COUNT_FOR_UBO) )
	{
		// In practice, there shouldn't be any game scene with a clipped polygon count that
		// would exceed POLYLIST_SIZE. But if for some reason there is, then we need to limit
		// the polygon count here. Please report if this happens!
		printf("OpenGL: Clipped poly count of %d exceeds %d. Please report!!!\n", (int)this->_clippedPolyCount, MAX_CLIPPED_POLY_COUNT_FOR_UBO);
		this->_clippedPolyCount = MAX_CLIPPED_POLY_COUNT_FOR_UBO;
	}
	
	// Set up the polygon states.
	this->_renderNeedsDepthEqualsTest = false;
	for (size_t i = 0, vertIndexCount = 0; i < this->_clippedPolyCount; i++)
	{
		const POLY &thePoly = *this->_clippedPolyList[i].poly;
		
		const size_t polyType = thePoly.type;
		const VERT vert[4] = {
			engine.vertList[thePoly.vertIndexes[0]],
			engine.vertList[thePoly.vertIndexes[1]],
			engine.vertList[thePoly.vertIndexes[2]],
			engine.vertList[thePoly.vertIndexes[3]]
		};
		
		for (size_t j = 0; j < polyType; j++)
		{
			const GLushort vertIndex = thePoly.vertIndexes[j];
			
			// While we're looping through our vertices, add each vertex index to
			// a buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
			// vertices here to convert them to GL_TRIANGLES, which are much easier
			// to work with and won't be deprecated in future OpenGL versions.
			OGLRef.vertIndexBuffer[vertIndexCount++] = vertIndex;
			if (!thePoly.isWireframe() && (thePoly.vtxFormat == GFX3D_QUADS || thePoly.vtxFormat == GFX3D_QUAD_STRIP))
			{
				if (j == 2)
				{
					OGLRef.vertIndexBuffer[vertIndexCount++] = vertIndex;
				}
				else if (j == 3)
				{
					OGLRef.vertIndexBuffer[vertIndexCount++] = thePoly.vertIndexes[0];
				}
			}
		}
		
		// Get the polygon's facing.
		const size_t n = polyType - 1;
		float facing = (vert[0].y + vert[n].y) * (vert[0].x - vert[n].x) +
		               (vert[1].y + vert[0].y) * (vert[1].x - vert[0].x) +
		               (vert[2].y + vert[1].y) * (vert[2].x - vert[1].x);
		
		for (size_t j = 2; j < n; j++)
		{
			facing += (vert[j+1].y + vert[j].y) * (vert[j+1].x - vert[j].x);
		}
		
		this->_renderNeedsDepthEqualsTest = this->_renderNeedsDepthEqualsTest || (thePoly.attribute.DepthEqualTest_Enable != 0);
		this->_isPolyFrontFacing[i] = (facing < 0);
		
		// Get the texture that is to be attached to this polygon.
		this->_textureList[i] = this->GetLoadedTextureFromPolygon(thePoly, this->_enableTextureSampling);
	}
	
	// Replace the entire buffer as a hint to the driver to orphan the buffer and avoid a synchronization cost.
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(OGLRef.vertIndexBuffer), OGLRef.vertIndexBuffer);
	
	OGLPolyStates *polyStates = this->_pendingPolyStates;
	
	if (OGLRef.uboPolyStatesID == 0)
	{
		// Some drivers seem to have problems with GL_TEXTURE_BUFFER used as the target for
		// glMapBufferRange() or glBufferSubData(), causing certain polygons to intermittently
		// flicker in certain games. Therefore, we'll use glMapBuffer() in this case in order
		// to prevent these glitches from happening.
		polyStates = (OGLPolyStates *)glMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY);
	}
	
	for (size_t i = 0; i < this->_clippedPolyCount; i++)
	{
		const POLY &thePoly = *this->_clippedPolyList[i].poly;
		
		// Get all of the polygon states that can be handled within the shader.
		const NDSTextureFormat packFormat = this->_textureList[i]->GetPackFormat();
		
		polyStates[i].packedState = 0;
		polyStates[i].PolygonID = thePoly.attribute.PolygonID;
		polyStates[i].PolygonMode = thePoly.attribute.Mode;
		
		polyStates[i].PolygonAlpha = (thePoly.isWireframe()) ? 0x1F : thePoly.attribute.Alpha;
		polyStates[i].IsWireframe = (thePoly.isWireframe()) ? 1 : 0;
		polyStates[i].EnableFog = (thePoly.attribute.Fog_Enable) ? 1 : 0;
		polyStates[i].SetNewDepthForTranslucent = (thePoly.attribute.TranslucentDepthWrite_Enable) ? 1 : 0;
		
		polyStates[i].EnableTexture = (this->_textureList[i]->IsSamplingEnabled()) ? 1 : 0;
		polyStates[i].TexSingleBitAlpha = (packFormat != TEXMODE_A3I5 && packFormat != TEXMODE_A5I3) ? 1 : 0;
		polyStates[i].TexSizeShiftS = thePoly.texParam.SizeShiftS; // Note that we are using the preshifted size of S
		polyStates[i].TexSizeShiftT = thePoly.texParam.SizeShiftT; // Note that we are using the preshifted size of T
	}
	
	if (OGLRef.uboPolyStatesID != 0)
	{
		// Replace the entire buffer as a hint to the driver to orphan the buffer and avoid a synchronization cost.
		glBindBuffer(GL_UNIFORM_BUFFER, OGLRef.uboPolyStatesID);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, MAX_CLIPPED_POLY_COUNT_FOR_UBO * sizeof(OGLPolyStates), this->_pendingPolyStates);
	}
	else
	{
		glUnmapBuffer(GL_TEXTURE_BUFFER);
	}
	
	// Set up the default draw call states.
	this->_geometryProgramFlags.EnableWDepth = (engine.renderState.wbuffer) ? 1 : 0;
	this->_geometryProgramFlags.EnableAlphaTest = (engine.renderState.enableAlphaTest) ? 1 : 0;
	this->_geometryProgramFlags.EnableTextureSampling = (this->_enableTextureSampling) ? 1 : 0;
	this->_geometryProgramFlags.EnableFog = (this->_enableFog) ? 1 : 0;
	this->_geometryProgramFlags.EnableEdgeMark = (this->_enableEdgeMark) ? 1 : 0;
	this->_geometryProgramFlags.ToonShadingMode = (engine.renderState.shading) ? 1 : 0;
	this->_geometryProgramFlags.NeedsDepthEqualsTest = (this->_renderNeedsDepthEqualsTest) ? 1 : 0;
	
	glUseProgram(OGLRef.programGeometryID[this->_geometryProgramFlags.value]);
	glUniform1i(OGLRef.uniformTexDrawOpaque[this->_geometryProgramFlags.value], GL_FALSE);
	glUniform1i(OGLRef.uniformPolyDrawShadow[this->_geometryProgramFlags.value], GL_FALSE);
	
	glDrawBuffers(3, GeometryDrawBuffersList[this->_geometryProgramFlags.DrawBuffersMode]);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	
	this->_needsZeroDstAlphaPass = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::RenderEdgeMarking(const u16 *colorTable, const bool useAntialias)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up the postprocessing states
	glViewport(0, 0, this->_framebufferWidth, this->_framebufferHeight);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
	glBindVertexArray(OGLRef.vaoPostprocessStatesID);
	
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
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glUseProgram(OGLRef.programEdgeMarkID);
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
		glUseProgram(OGLRef.programEdgeMarkID);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glEnable(GL_BLEND);
		glDisable(GL_STENCIL_TEST);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}
	
	glBindVertexArray(0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::RenderFog(const u8 *densityTable, const u32 color, const u16 offset, const u8 shift, const bool alphaOnly)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	OGLFogProgramKey fogProgramKey;
	fogProgramKey.key = 0;
	fogProgramKey.offset = offset;
	fogProgramKey.shift = shift;
	
	std::map<u32, OGLFogShaderID>::iterator it = this->_fogProgramMap.find(fogProgramKey.key);
	if (it == this->_fogProgramMap.end())
	{
		Render3DError error = this->CreateFogProgram(fogProgramKey, FogVtxShader_150, FogFragShader_150);
		if (error != OGLERROR_NOERR)
		{
			return error;
		}
	}
	
	OGLFogShaderID shaderID = this->_fogProgramMap[fogProgramKey.key];
	
	glDrawBuffer(GL_COLOR_ATTACHMENT3);
	glUseProgram(shaderID.program);
	glViewport(0, 0, this->_framebufferWidth, this->_framebufferHeight);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboPostprocessVtxID);
	glBindVertexArray(OGLRef.vaoPostprocessStatesID);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
	
	this->_lastTextureDrawTarget = OGLTextureUnitID_FinalColor;
	
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

Render3DError OpenGLRenderer_3_2::ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	this->UploadClearImage(colorBuffer, depthBuffer, fogBuffer, opaquePolyID);
	
	glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.fboClearImageID);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.fboRenderID);
	glDrawBuffers(3, GeometryDrawBuffersList[this->_geometryProgramFlags.DrawBuffersMode]);
	
	if (this->_enableEdgeMark)
	{
		// Clear the polygon ID buffer
		const GLfloat oglPolyID[4] = {(GLfloat)opaquePolyID/63.0f, 0.0f, 0.0f, 1.0f};
		glClearBufferfv(GL_COLOR, 1, oglPolyID);
	}
	
	if (this->_enableFog)
	{
		// Blit the fog buffer
		glReadBuffer(GL_COLOR_ATTACHMENT2);
		glDrawBuffer(GL_COLOR_ATTACHMENT2);
		glBlitFramebuffer(0, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GPU_FRAMEBUFFER_NATIVE_WIDTH, 0, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	
	// Blit the color and depth/stencil buffers. Do this last so that color attachment 0 is set to the read FBO.
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glBlitFramebuffer(0, GPU_FRAMEBUFFER_NATIVE_HEIGHT, GPU_FRAMEBUFFER_NATIVE_WIDTH, 0, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboRenderID);
	
	OGLRef.selectedRenderingFBO = (this->_enableMultisampledRendering) ? OGLRef.fboMSIntermediateRenderID : OGLRef.fboRenderID;
	if (OGLRef.selectedRenderingFBO == OGLRef.fboMSIntermediateRenderID)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.fboRenderID);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
		glDrawBuffers(3, GeometryDrawBuffersList[this->_geometryProgramFlags.DrawBuffersMode]);
		
		if (this->_enableEdgeMark)
		{
			// Clear the polygon ID buffer
			const GLfloat oglPolyID[4] = {(GLfloat)opaquePolyID/63.0f, 0.0f, 0.0f, 1.0f};
			glClearBufferfv(GL_COLOR, 1, oglPolyID);
		}
		
		if (this->_enableFog)
		{
			// Blit the fog buffer
			glReadBuffer(GL_COLOR_ATTACHMENT2);
			glDrawBuffer(GL_COLOR_ATTACHMENT2);
			glBlitFramebuffer(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		
		// Blit the color and depth/stencil buffers. Do this last so that color attachment 0 is set to the read FBO.
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(0, 0, this->_framebufferWidth, this->_framebufferHeight, 0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
		
		glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::ClearUsingValues(const FragmentColor &clearColor6665, const FragmentAttributes &clearAttributes)
{
	OGLRenderRef &OGLRef = *this->ref;
	OGLRef.selectedRenderingFBO = (this->_enableMultisampledRendering) ? OGLRef.fboMSIntermediateRenderID : OGLRef.fboRenderID;
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
	glDrawBuffers(3, GeometryDrawBuffersList[this->_geometryProgramFlags.DrawBuffersMode]);
	
	const GLfloat oglColor[4] = {divide6bitBy63_LUT[clearColor6665.r], divide6bitBy63_LUT[clearColor6665.g], divide6bitBy63_LUT[clearColor6665.b], divide5bitBy31_LUT[clearColor6665.a]};
	glClearBufferfv(GL_COLOR, 0, oglColor); // texGColorID
	glClearBufferfi(GL_DEPTH_STENCIL, 0, (GLfloat)clearAttributes.depth / (GLfloat)0x00FFFFFF, clearAttributes.opaquePolyID);
	
	if (this->_enableEdgeMark)
	{
		const GLfloat oglPolyID[4] = {(GLfloat)clearAttributes.opaquePolyID/63.0f, 0.0f, 0.0f, 1.0f};
		glClearBufferfv(GL_COLOR, 1, oglPolyID); // texGPolyID
	}
	
	if (this->_enableFog)
	{
		const GLfloat oglFogAttr[4] = {(GLfloat)clearAttributes.isFogged, 0.0f, 0.0f, 1.0f};
		
		if (GeometryDrawBuffersList[this->_geometryProgramFlags.DrawBuffersMode][1] == GL_COLOR_ATTACHMENT2)
		{
			glClearBufferfv(GL_COLOR, 1, oglFogAttr);
		}
		else if (GeometryDrawBuffersList[this->_geometryProgramFlags.DrawBuffersMode][2] == GL_COLOR_ATTACHMENT2)
		{
			glClearBufferfv(GL_COLOR, 2, oglFogAttr);
		}
	}
	
	this->_needsZeroDstAlphaPass = (clearColor6665.a == 0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_3_2::SetPolygonIndex(const size_t index)
{
	this->_currentPolyIndex = index;
	glUniform1i(this->ref->uniformPolyStateIndex[this->_geometryProgramFlags.value], index);
	
	if (this->_syncBufferSetup != NULL)
	{
		glWaitSync(this->_syncBufferSetup, 0, GL_TIMEOUT_IGNORED);
		glDeleteSync(this->_syncBufferSetup);
		this->_syncBufferSetup = NULL;
	}
}

Render3DError OpenGLRenderer_3_2::SetupPolygon(const POLY &thePoly, bool treatAsTranslucent, bool willChangeStencilBuffer)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up depth test mode
	glDepthFunc((thePoly.attribute.DepthEqualTest_Enable) ? GL_EQUAL : GL_LESS);
	glUniform1i(OGLRef.uniformPolyDepthOffsetMode[this->_geometryProgramFlags.value], 0);
	
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
	OGLRenderRef &OGLRef = *this->ref;
	
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
	
	const size_t newFramebufferColorSizeBytes = w * h * sizeof(FragmentColor);
	glBufferData(GL_PIXEL_PACK_BUFFER, newFramebufferColorSizeBytes, NULL, GL_STREAM_READ);
	
	if (this->_mappedFramebuffer != NULL)
	{
		this->_mappedFramebuffer = (FragmentColor *__restrict)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		glFinish();
	}
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FinalColor);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texFinalColorID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_DepthStencil);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthStencilID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, w, h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GColor);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGColorID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_GPolyID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_FogAttr);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
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
	
	this->CreateEdgeMarkProgram(EdgeMarkVtxShader_150, EdgeMarkFragShader_150);
	this->CreateFramebufferOutput6665Program(0, FramebufferOutputVtxShader_150, FramebufferOutput6665FragShader_150);
	this->CreateFramebufferOutput6665Program(1, FramebufferOutputVtxShader_150, FramebufferOutput6665FragShader_150);
	
	if (this->isSampleShadingSupported)
	{
		Render3DError shaderError = this->CreateMSGeometryZeroDstAlphaProgram(MSGeometryZeroDstAlphaPixelMaskVtxShader_150, MSGeometryZeroDstAlphaPixelMaskFragShader_150);
		this->willUsePerSampleZeroDstPass = (shaderError == OGLERROR_NOERR);
	}
	
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
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboRenderID);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glClearBufferfv(GL_COLOR, 0, oglColor);
	
	if (this->_mappedFramebuffer != NULL)
	{
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		this->_mappedFramebuffer = NULL;
	}
	
	glReadPixels(0, 0, this->_framebufferWidth, this->_framebufferHeight, GL_BGRA, GL_UNSIGNED_BYTE, 0);
	
	ENDGL();
	
	this->_pixelReadNeedsFinish = true;
	return OGLERROR_NOERR;
}
