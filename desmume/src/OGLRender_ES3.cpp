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

#include "OGLRender_ES3.h"

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


// Vertex shader for geometry, GLSL ES 3.00
static const char *GeometryVtxShader_ES300 = {"\
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
uniform highp isamplerBuffer PolyStates;\n\
#else\n\
uniform highp isampler2D PolyStates;\n\
#endif\n\
uniform mediump int polyIndex;\n\
uniform bool polyDrawShadow;\n\
\n\
out vec2 vtxTexCoord;\n\
out vec4 vtxColor;\n\
flat out lowp int polyID;\n\
flat out lowp int polyMode;\n\
flat out lowp int polyIsWireframe;\n\
flat out lowp int polyEnableFog;\n\
flat out lowp int polySetNewDepthForTranslucent;\n\
flat out lowp int polyEnableTexture;\n\
flat out lowp int texSingleBitAlpha;\n\
flat out lowp int polyIsBackFacing;\n\
flat out lowp int isPolyDrawable;\n\
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

// Fragment shader for geometry, GLSL ES 3.00
static const char *GeometryFragShader_ES300 = {"\
in vec2 vtxTexCoord;\n\
in vec4 vtxColor;\n\
flat in lowp int polyID;\n\
flat in lowp int polyMode;\n\
flat in lowp int polyIsWireframe;\n\
flat in lowp int polyEnableFog;\n\
flat in lowp int polySetNewDepthForTranslucent;\n\
flat in lowp int polyEnableTexture;\n\
flat in lowp int texSingleBitAlpha;\n\
flat in lowp int polyIsBackFacing;\n\
flat in lowp int isPolyDrawable;\n\
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

void OGLLoadEntryPoints_ES_3_0()
{
	OGLLoadEntryPoints_3_2();
}

void OGLCreateRenderer_ES_3_0(OpenGLRenderer **rendererPtr)
{
	if (IsOpenGLDriverVersionSupported(3, 0, 0))
	{
		*rendererPtr = new OpenGLESRenderer_3_0;
		(*rendererPtr)->SetVersion(3, 0, 0);
	}
}

OpenGLESRenderer_3_0::OpenGLESRenderer_3_0()
{
	_variantID = OpenGLVariantID_ES_3_0;
}

Render3DError OpenGLESRenderer_3_0::InitExtensions()
{
	OGLRenderRef &OGLRef = *this->ref;
	Render3DError error = OGLERROR_NOERR;
	
	// Get OpenGL extensions
	std::set<std::string> oglExtensionSet;
	this->GetExtensionSet(&oglExtensionSet);
	
	// OpenGL ES 3.0 should fully support FBOs, so we don't need the default framebuffer.
	// However, OpenGL ES has traditionally required some kind of surface buffer attached
	// to the context before using it. We don't want it, nor would we ever use it here.
	// Therefore, check if our context supports being surfaceless before doing anything else,
	// as this works as a kind of compatibility check.
	if (!this->IsExtensionPresent(&oglExtensionSet, "GL_OES_surfaceless_context"))
	{
		INFO("OpenGL ES: Client contexts are not expected to have any surfaces attached\n");
		INFO("           to the default framebuffer. The fact that the context does not\n");
		INFO("           work surfaceless may indicate an error in the context creation,\n");
		INFO("           or that your platform's context creation is too old.\n");
		error = OGLERROR_FEATURE_UNSUPPORTED;
		return error;
	}
	
	// Get host GPU device properties
	GLint maxUBOSize = 0;
	glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUBOSize);
	this->_is64kUBOSupported = (maxUBOSize >= 65536);
	
	// TBOs are only supported in ES 3.2.
	this->_isTBOSupported = IsOpenGLDriverVersionSupported(3, 2, 0);
	
	// Fixed locations in shaders are supported in ES 3.0 by default.
	this->_isShaderFixedLocationSupported = true;
	
	GLfloat maxAnisotropyOGL = 1.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropyOGL);
	this->_deviceInfo.maxAnisotropy = (float)maxAnisotropyOGL;
	
	// OpenGL ES 3.0 needs to look up the best format and data type for glReadPixels.
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
	
	// OpenGL ES v3.0 should have all the necessary features to be able to flip and convert the framebuffer.
	this->willFlipAndConvertFramebufferOnGPU = true;
	
	this->_enableTextureSmoothing = CommonSettings.GFX3D_Renderer_TextureSmoothing;
	this->_emulateShadowPolygon = CommonSettings.OpenGL_Emulation_ShadowPolygon;
	this->_emulateSpecialZeroAlphaBlending = CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending;
	this->_emulateNDSDepthCalculation = CommonSettings.OpenGL_Emulation_NDSDepthCalculation;
	this->_emulateDepthLEqualPolygonFacing = CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing;
	
	// Load and create shaders. Return on any error, since ES 3.0 makes shaders mandatory.
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
	
	INFO("OpenGL ES: Successfully created geometry shaders.\n");
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
		INFO("OpenGL ES: Driver does not support at least 2x multisampled FBOs.\n");
	}
	
	this->_isDepthLEqualPolygonFacingSupported = true;
	this->_enableMultisampledRendering = ((this->_selectedMultisampleSize >= 2) && this->isMultisampledFBOSupported);
	
	this->InitFinalRenderStates(&oglExtensionSet); // This must be done last
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLESRenderer_3_0::CreateGeometryPrograms()
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
		// but requires a GPU that supports 64k UBO transfers.
		if (OGLRef.uboPolyStatesID == 0)
		{
			glGenBuffers(1, &OGLRef.uboPolyStatesID);
			glBindBuffer(GL_UNIFORM_BUFFER, OGLRef.uboPolyStatesID);
			glBufferData(GL_UNIFORM_BUFFER, MAX_CLIPPED_POLY_COUNT_FOR_UBO * sizeof(OGLPolyStates), NULL, GL_DYNAMIC_DRAW);
			glBindBufferBase(GL_UNIFORM_BUFFER, OGLBindingPointID_PolyStates, OGLRef.uboPolyStatesID);
		}
	}
#ifdef GL_ES_VERSION_3_2
	else if (this->_isTBOSupported)
	{
		// If for some reason the GPU doesn't support 64k UBOs but still supports OpenGL ES 3.2,
		// then use a TBO as the second fastest method.
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
		// integer texture.
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
	shaderHeader << "#version 300 es\n";
	shaderHeader << "precision highp float;\n";
	shaderHeader << "precision highp int;\n";
	shaderHeader << "\n";
	
	std::stringstream vsHeader;
	vsHeader << "#define IN_VTX_POSITION layout (location = "  << OGLVertexAttributeID_Position  << ") in\n";
	vsHeader << "#define IN_VTX_TEXCOORD0 layout (location = " << OGLVertexAttributeID_TexCoord0 << ") in\n";
	vsHeader << "#define IN_VTX_COLOR layout (location = "     << OGLVertexAttributeID_Color     << ") in\n";
	vsHeader << "\n";
	vsHeader << "#define IS_USING_UBO_POLY_STATES " << ((OGLRef.uboPolyStatesID != 0) ? 1 : 0) << "\n";
	vsHeader << "#define IS_USING_TBO_POLY_STATES " << ((OGLRef.tboPolyStatesID != 0) ? 1 : 0) << "\n";
	vsHeader << "#define DEPTH_EQUALS_TEST_TOLERANCE " << DEPTH_EQUALS_TEST_TOLERANCE << ".0\n";
	vsHeader << "\n";
	
	std::string vtxShaderCode  = shaderHeader.str() + vsHeader.str() + std::string(GeometryVtxShader_ES300);
	
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
		
		std::string fragShaderCode = shaderHeader.str() + shaderFlags.str() + std::string(GeometryFragShader_ES300);
		
		error = this->ShaderProgramCreate(OGLRef.vertexGeometryShaderID,
										  OGLRef.fragmentGeometryShaderID[flagsValue],
										  OGLRef.programGeometryID[flagsValue],
										  vtxShaderCode.c_str(),
										  fragShaderCode.c_str());
		if (error != OGLERROR_NOERR)
		{
			INFO("OpenGL ES: Failed to create the GEOMETRY shader program.\n");
			glUseProgram(0);
			this->DestroyGeometryPrograms();
			return error;
		}
		
		glLinkProgram(OGLRef.programGeometryID[flagsValue]);
		if (!this->ValidateShaderProgramLink(OGLRef.programGeometryID[flagsValue]))
		{
			INFO("OpenGL ES: Failed to link the GEOMETRY shader program.\n");
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

Render3DError OpenGLESRenderer_3_0::CreateGeometryZeroDstAlphaProgram(const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (vtxShaderCString == NULL) || (fragShaderCString == NULL) )
	{
		return error;
	}
	
	std::stringstream shaderHeader;
	shaderHeader << "#version 300 es\n";
	shaderHeader << "precision highp float;\n";
	shaderHeader << "precision highp int;\n";
	shaderHeader << "\n";
	
	std::stringstream vsHeader;
	vsHeader << "#define IN_VTX_POSITION layout (location = "  << OGLVertexAttributeID_Position  << ") in\n";
	vsHeader << "#define IN_VTX_TEXCOORD0 layout (location = " << OGLVertexAttributeID_TexCoord0 << ") in\n";
	vsHeader << "#define IN_VTX_COLOR layout (location = "     << OGLVertexAttributeID_Color     << ") in\n";
	
	std::string vtxShaderCode  = shaderHeader.str() + vsHeader.str() + std::string(vtxShaderCString);
	std::string fragShaderCode = shaderHeader.str() + std::string(fragShaderCString);
	
	error = this->ShaderProgramCreate(OGLRef.vtxShaderGeometryZeroDstAlphaID,
									  OGLRef.fragShaderGeometryZeroDstAlphaID,
									  OGLRef.programGeometryZeroDstAlphaID,
									  vtxShaderCode.c_str(),
									  fragShaderCode.c_str());
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL ES: Failed to create the GEOMETRY ZERO DST ALPHA shader program.\n");
		glUseProgram(0);
		this->DestroyGeometryZeroDstAlphaProgram();
		return error;
	}
	
	glLinkProgram(OGLRef.programGeometryZeroDstAlphaID);
	if (!this->ValidateShaderProgramLink(OGLRef.programGeometryZeroDstAlphaID))
	{
		INFO("OpenGL ES: Failed to link the GEOMETRY ZERO DST ALPHA shader program.\n");
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

Render3DError OpenGLESRenderer_3_0::CreateEdgeMarkProgram(const bool isMultisample, const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (vtxShaderCString == NULL) || (fragShaderCString == NULL) )
	{
		return error;
	}
	
	std::stringstream shaderHeader;
	shaderHeader << "#version 300 es\n";
	shaderHeader << "precision highp float;\n";
	shaderHeader << "precision highp int;\n";
	shaderHeader << "\n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_X " << this->_framebufferWidth  << ".0 \n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_Y " << this->_framebufferHeight << ".0 \n";
	shaderHeader << "\n";
	
	std::stringstream vsHeader;
	vsHeader << "#define IN_VTX_POSITION layout (location = "  << OGLVertexAttributeID_Position  << ") in\n";
	vsHeader << "#define IN_VTX_TEXCOORD0 layout (location = " << OGLVertexAttributeID_TexCoord0 << ") in\n";
	vsHeader << "#define IN_VTX_COLOR layout (location = "     << OGLVertexAttributeID_Color     << ") in\n";
	
	std::stringstream fsHeader;
	fsHeader << "#define OUT_COLOR layout (location = 0) out\n";
	
	std::string vtxShaderCode  = shaderHeader.str() + vsHeader.str() + std::string(vtxShaderCString);
	std::string fragShaderCode = shaderHeader.str() + fsHeader.str() + std::string(fragShaderCString);
	
	error = this->ShaderProgramCreate(OGLRef.vertexEdgeMarkShaderID,
									  OGLRef.fragmentEdgeMarkShaderID,
									  OGLRef.programEdgeMarkID,
									  vtxShaderCode.c_str(),
									  fragShaderCode.c_str());
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL ES: Failed to create the EDGE MARK shader program.\n");
		glUseProgram(0);
		this->DestroyEdgeMarkProgram();
		return error;
	}
	
	glLinkProgram(OGLRef.programEdgeMarkID);
	if (!this->ValidateShaderProgramLink(OGLRef.programEdgeMarkID))
	{
		INFO("OpenGL ES: Failed to link the EDGE MARK shader program.\n");
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

Render3DError OpenGLESRenderer_3_0::CreateFogProgram(const OGLFogProgramKey fogProgramKey, const bool isMultisample, const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if (vtxShaderCString == NULL)
	{
		INFO("OpenGL ES: The FOG vertex shader is unavailable.\n");
		error = OGLERROR_VERTEX_SHADER_PROGRAM_LOAD_ERROR;
		return error;
	}
	else if (fragShaderCString == NULL)
	{
		INFO("OpenGL ES: The FOG fragment shader is unavailable.\n");
		error = OGLERROR_FRAGMENT_SHADER_PROGRAM_LOAD_ERROR;
		return error;
	}
	
	const s32 fogOffset = fogProgramKey.offset;
	const GLfloat fogOffsetf = (GLfloat)fogOffset / 32767.0f;
	const s32 fogStep = 0x0400 >> fogProgramKey.shift;
	
	std::stringstream shaderHeader;
	shaderHeader << "#version 300 es\n";
	shaderHeader << "precision highp float;\n";
	shaderHeader << "precision highp int;\n";
	shaderHeader << "\n";
	
	std::stringstream vsHeader;
	vsHeader << "#define IN_VTX_POSITION layout (location = "  << OGLVertexAttributeID_Position  << ") in\n";
	vsHeader << "#define IN_VTX_TEXCOORD0 layout (location = " << OGLVertexAttributeID_TexCoord0 << ") in\n";
	vsHeader << "#define IN_VTX_COLOR layout (location = "     << OGLVertexAttributeID_Color     << ") in\n";
	
	std::stringstream fsHeader;
	fsHeader << "#define FOG_OFFSET " << fogOffset << "\n";
	fsHeader << "#define FOG_OFFSETF " << fogOffsetf << (((fogOffsetf == 0.0f) || (fogOffsetf == 1.0f)) ? ".0" : "") << "\n";
	fsHeader << "#define FOG_STEP " << fogStep << "\n";
	fsHeader << "\n";
	fsHeader << "#define OUT_COLOR layout (location = 0) out\n";
	
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
		INFO("OpenGL ES: Failed to create the FOG shader program.\n");
		glUseProgram(0);
		this->DestroyFogProgram(fogProgramKey);
		return error;
	}
	
	glLinkProgram(shaderID.program);
	if (!this->ValidateShaderProgramLink(shaderID.program))
	{
		INFO("OpenGL ES: Failed to link the FOG shader program.\n");
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

Render3DError OpenGLESRenderer_3_0::CreateFramebufferOutput6665Program(const size_t outColorIndex, const char *vtxShaderCString, const char *fragShaderCString)
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	if ( (vtxShaderCString == NULL) || (fragShaderCString == NULL) )
	{
		return error;
	}
	
	std::stringstream shaderHeader;
	shaderHeader << "#version 300 es\n";
	shaderHeader << "precision highp float;\n";
	shaderHeader << "precision highp int;\n";
	shaderHeader << "\n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_X " << this->_framebufferWidth  << ".0 \n";
	shaderHeader << "#define FRAMEBUFFER_SIZE_Y " << this->_framebufferHeight << ".0 \n";
	shaderHeader << "\n";
	
	std::stringstream vsHeader;
	vsHeader << "#define IN_VTX_POSITION layout (location = "  << OGLVertexAttributeID_Position  << ") in\n";
	vsHeader << "#define IN_VTX_TEXCOORD0 layout (location = " << OGLVertexAttributeID_TexCoord0 << ") in\n";
	vsHeader << "#define IN_VTX_COLOR layout (location = "     << OGLVertexAttributeID_Color     << ") in\n";
	
	std::stringstream fsHeader;
	fsHeader << "#define OUT_COLOR layout (location = 0) out\n";
	
	std::string vtxShaderCode  = shaderHeader.str() + vsHeader.str() + std::string(vtxShaderCString);
	std::string fragShaderCode = shaderHeader.str() + fsHeader.str() + std::string(fragShaderCString);
	
	error = this->ShaderProgramCreate(OGLRef.vertexFramebufferOutput6665ShaderID,
									  OGLRef.fragmentFramebufferRGBA6665OutputShaderID,
									  OGLRef.programFramebufferRGBA6665OutputID[outColorIndex],
									  vtxShaderCode.c_str(),
									  fragShaderCode.c_str());
	if (error != OGLERROR_NOERR)
	{
		INFO("OpenGL ES: Failed to create the FRAMEBUFFER OUTPUT RGBA6665 shader program.\n");
		glUseProgram(0);
		this->DestroyFramebufferOutput6665Programs();
		return error;
	}
	
	glLinkProgram(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex]);
	if (!this->ValidateShaderProgramLink(OGLRef.programFramebufferRGBA6665OutputID[outColorIndex]))
	{
		INFO("OpenGL ES: Failed to link the FRAMEBUFFER OUTPUT RGBA6665 shader program.\n");
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
