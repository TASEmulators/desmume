/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2021 DeSmuME team

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

#ifndef OGLRENDER_H
#define OGLRENDER_H

#include <map>
#include <queue>
#include <set>
#include <string>
#include "render3D.h"
#include "types.h"

#ifndef OGLRENDER_3_2_H

#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <GL/gl.h>
	#include <GL/glext.h>

	#define OGLEXT(procPtr, func)		procPtr func = NULL;
	#define INITOGLEXT(procPtr, func)	func = (procPtr)wglGetProcAddress(#func);
	#define EXTERNOGLEXT(procPtr, func)	extern procPtr func;
#elif defined(__APPLE__)
	#include <OpenGL/gl.h>
	#include <OpenGL/glext.h>

	// Ignore dynamic linking on Apple OS
	#define OGLEXT(procPtr, func)
	#define INITOGLEXT(procPtr, func)
	#define EXTERNOGLEXT(procPtr, func)

	// We're not exactly committing to OpenGL 3.2 Core Profile just yet, so redefine APPLE
	// extensions as a temporary measure.
	#if defined(GL_APPLE_vertex_array_object) && !defined(GL_ARB_vertex_array_object)
		#define glGenVertexArrays(n, ids)		glGenVertexArraysAPPLE(n, ids)
		#define glBindVertexArray(id)			glBindVertexArrayAPPLE(id)
		#define glDeleteVertexArrays(n, ids)	glDeleteVertexArraysAPPLE(n, ids)
	#endif
#else
	#include <GL/gl.h>
	#include <GL/glext.h>
	#include <GL/glx.h>

	/* This is a workaround needed to compile against nvidia GL headers */
	#ifndef GL_ALPHA_BLEND_EQUATION_ATI
		#undef GL_VERSION_1_3
	#endif

	#define OGLEXT(procPtr, func)		procPtr func = NULL;
	#define INITOGLEXT(procPtr, func)	func = (procPtr)glXGetProcAddress((const GLubyte *) #func);
	#define EXTERNOGLEXT(procPtr, func)	extern procPtr func;
#endif

// Check minimum OpenGL header version
#if !defined(GL_VERSION_2_1)
	#if defined(GL_VERSION_2_0)
		#warning Using OpenGL v2.0 headers with v2.1 overrides. Some features will be disabled.

		#if !defined(GL_ARB_framebuffer_object)
			// Overrides for GL_EXT_framebuffer_blit
			#if !defined(GL_EXT_framebuffer_blit)
				#define GL_READ_FRAMEBUFFER_EXT		GL_FRAMEBUFFER_EXT
				#define GL_DRAW_FRAMEBUFFER_EXT		GL_FRAMEBUFFER_EXT
				#define glBlitFramebufferEXT(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter)
			#endif

			// Overrides for GL_EXT_framebuffer_multisample
			#if !defined(GL_EXT_framebuffer_multisample)
				#define GL_MAX_SAMPLES_EXT 0
				#define glRenderbufferStorageMultisampleEXT(target, samples, internalformat, width, height)
			#endif

			// Overrides for GL_ARB_pixel_buffer_object
			#if !defined(GL_PIXEL_PACK_BUFFER) && defined(GL_PIXEL_PACK_BUFFER_ARB)
				#define GL_PIXEL_PACK_BUFFER GL_PIXEL_PACK_BUFFER_ARB
			#endif
		#endif
	#else
		#error OpenGL requires v2.1 headers or later.
	#endif
#endif

// Textures
#if !defined(GLX_H)
EXTERNOGLEXT(PFNGLACTIVETEXTUREPROC, glActiveTexture) // Core in v1.3
EXTERNOGLEXT(PFNGLACTIVETEXTUREARBPROC, glActiveTextureARB)
#endif

// Blending
#if !defined(GLX_H)
EXTERNOGLEXT(PFNGLBLENDEQUATIONPROC, glBlendEquation) // Core in v1.2
#endif
EXTERNOGLEXT(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate) // Core in v1.4
EXTERNOGLEXT(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate) // Core in v2.0

EXTERNOGLEXT(PFNGLBLENDFUNCSEPARATEEXTPROC, glBlendFuncSeparateEXT)
EXTERNOGLEXT(PFNGLBLENDEQUATIONSEPARATEEXTPROC, glBlendEquationSeparateEXT)

// Shaders
EXTERNOGLEXT(PFNGLCREATESHADERPROC, glCreateShader) // Core in v2.0
EXTERNOGLEXT(PFNGLSHADERSOURCEPROC, glShaderSource) // Core in v2.0
EXTERNOGLEXT(PFNGLCOMPILESHADERPROC, glCompileShader) // Core in v2.0
EXTERNOGLEXT(PFNGLCREATEPROGRAMPROC, glCreateProgram) // Core in v2.0
EXTERNOGLEXT(PFNGLATTACHSHADERPROC, glAttachShader) // Core in v2.0
EXTERNOGLEXT(PFNGLDETACHSHADERPROC, glDetachShader) // Core in v2.0
EXTERNOGLEXT(PFNGLLINKPROGRAMPROC, glLinkProgram) // Core in v2.0
EXTERNOGLEXT(PFNGLUSEPROGRAMPROC, glUseProgram) // Core in v2.0
EXTERNOGLEXT(PFNGLGETSHADERIVPROC, glGetShaderiv) // Core in v2.0
EXTERNOGLEXT(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) // Core in v2.0
EXTERNOGLEXT(PFNGLDELETESHADERPROC, glDeleteShader) // Core in v2.0
EXTERNOGLEXT(PFNGLDELETEPROGRAMPROC, glDeleteProgram) // Core in v2.0
EXTERNOGLEXT(PFNGLGETPROGRAMIVPROC, glGetProgramiv) // Core in v2.0
EXTERNOGLEXT(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) // Core in v2.0
EXTERNOGLEXT(PFNGLVALIDATEPROGRAMPROC, glValidateProgram) // Core in v2.0
EXTERNOGLEXT(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) // Core in v2.0
EXTERNOGLEXT(PFNGLUNIFORM1IPROC, glUniform1i) // Core in v2.0
EXTERNOGLEXT(PFNGLUNIFORM1IVPROC, glUniform1iv) // Core in v2.0
EXTERNOGLEXT(PFNGLUNIFORM1FPROC, glUniform1f) // Core in v2.0
EXTERNOGLEXT(PFNGLUNIFORM1FVPROC, glUniform1fv) // Core in v2.0
EXTERNOGLEXT(PFNGLUNIFORM2FPROC, glUniform2f) // Core in v2.0
EXTERNOGLEXT(PFNGLUNIFORM4FPROC, glUniform4f) // Core in v2.0
EXTERNOGLEXT(PFNGLUNIFORM4FVPROC, glUniform4fv) // Core in v2.0
EXTERNOGLEXT(PFNGLDRAWBUFFERSPROC, glDrawBuffers) // Core in v2.0

// Generic vertex attributes
EXTERNOGLEXT(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) // Core in v2.0
EXTERNOGLEXT(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) // Core in v2.0
EXTERNOGLEXT(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) // Core in v2.0
EXTERNOGLEXT(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) // Core in v2.0

// VAO
EXTERNOGLEXT(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)
EXTERNOGLEXT(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)
EXTERNOGLEXT(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)

// VBO and PBO
EXTERNOGLEXT(PFNGLGENBUFFERSARBPROC, glGenBuffersARB)
EXTERNOGLEXT(PFNGLDELETEBUFFERSARBPROC, glDeleteBuffersARB)
EXTERNOGLEXT(PFNGLBINDBUFFERARBPROC, glBindBufferARB)
EXTERNOGLEXT(PFNGLBUFFERDATAARBPROC, glBufferDataARB)
EXTERNOGLEXT(PFNGLBUFFERSUBDATAARBPROC, glBufferSubDataARB)
EXTERNOGLEXT(PFNGLMAPBUFFERARBPROC, glMapBufferARB)
EXTERNOGLEXT(PFNGLUNMAPBUFFERARBPROC, glUnmapBufferARB)

EXTERNOGLEXT(PFNGLGENBUFFERSPROC, glGenBuffers) // Core in v1.5
EXTERNOGLEXT(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) // Core in v1.5
EXTERNOGLEXT(PFNGLBINDBUFFERPROC, glBindBuffer) // Core in v1.5
EXTERNOGLEXT(PFNGLBUFFERDATAPROC, glBufferData) // Core in v1.5
EXTERNOGLEXT(PFNGLBUFFERSUBDATAPROC, glBufferSubData) // Core in v1.5
EXTERNOGLEXT(PFNGLMAPBUFFERPROC, glMapBuffer) // Core in v1.5
EXTERNOGLEXT(PFNGLUNMAPBUFFERPROC, glUnmapBuffer) // Core in v1.5

// FBO
EXTERNOGLEXT(PFNGLGENFRAMEBUFFERSEXTPROC, glGenFramebuffersEXT)
EXTERNOGLEXT(PFNGLBINDFRAMEBUFFEREXTPROC, glBindFramebufferEXT)
EXTERNOGLEXT(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC, glFramebufferRenderbufferEXT)
EXTERNOGLEXT(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC, glFramebufferTexture2DEXT)
EXTERNOGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, glCheckFramebufferStatusEXT)
EXTERNOGLEXT(PFNGLDELETEFRAMEBUFFERSEXTPROC, glDeleteFramebuffersEXT)
EXTERNOGLEXT(PFNGLBLITFRAMEBUFFEREXTPROC, glBlitFramebufferEXT)

// Multisampled FBO
EXTERNOGLEXT(PFNGLGENRENDERBUFFERSEXTPROC, glGenRenderbuffersEXT)
EXTERNOGLEXT(PFNGLBINDRENDERBUFFEREXTPROC, glBindRenderbufferEXT)
EXTERNOGLEXT(PFNGLRENDERBUFFERSTORAGEEXTPROC, glRenderbufferStorageEXT)
EXTERNOGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC, glRenderbufferStorageMultisampleEXT)
EXTERNOGLEXT(PFNGLDELETERENDERBUFFERSEXTPROC, glDeleteRenderbuffersEXT)

#else // OGLRENDER_3_2_H

// Basic Functions
EXTERNOGLEXT(PFNGLGETSTRINGIPROC, glGetStringi) // Core in v3.0
EXTERNOGLEXT(PFNGLCLEARBUFFERFVPROC, glClearBufferfv) // Core in v3.0
EXTERNOGLEXT(PFNGLCLEARBUFFERFIPROC, glClearBufferfi) // Core in v3.0

// Textures
#if !defined(GLX_H)
EXTERNOGLEXT(PFNGLACTIVETEXTUREPROC, glActiveTexture) // Core in v1.3
#endif

// Blending
#if !defined(GLX_H)
EXTERNOGLEXT(PFNGLBLENDEQUATIONPROC, glBlendEquation) // Core in v1.2
#endif
EXTERNOGLEXT(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate) // Core in v1.4
EXTERNOGLEXT(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate) // Core in v2.0

// Shaders
EXTERNOGLEXT(PFNGLCREATESHADERPROC, glCreateShader) // Core in v2.0
EXTERNOGLEXT(PFNGLSHADERSOURCEPROC, glShaderSource) // Core in v2.0
EXTERNOGLEXT(PFNGLCOMPILESHADERPROC, glCompileShader) // Core in v2.0
EXTERNOGLEXT(PFNGLCREATEPROGRAMPROC, glCreateProgram) // Core in v2.0
EXTERNOGLEXT(PFNGLATTACHSHADERPROC, glAttachShader) // Core in v2.0
EXTERNOGLEXT(PFNGLDETACHSHADERPROC, glDetachShader) // Core in v2.0
EXTERNOGLEXT(PFNGLLINKPROGRAMPROC, glLinkProgram) // Core in v2.0
EXTERNOGLEXT(PFNGLUSEPROGRAMPROC, glUseProgram) // Core in v2.0
EXTERNOGLEXT(PFNGLGETSHADERIVPROC, glGetShaderiv) // Core in v2.0
EXTERNOGLEXT(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) // Core in v2.0
EXTERNOGLEXT(PFNGLDELETESHADERPROC, glDeleteShader) // Core in v2.0
EXTERNOGLEXT(PFNGLDELETEPROGRAMPROC, glDeleteProgram) // Core in v2.0
EXTERNOGLEXT(PFNGLGETPROGRAMIVPROC, glGetProgramiv) // Core in v2.0
EXTERNOGLEXT(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) // Core in v2.0
EXTERNOGLEXT(PFNGLVALIDATEPROGRAMPROC, glValidateProgram) // Core in v2.0
EXTERNOGLEXT(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) // Core in v2.0
EXTERNOGLEXT(PFNGLUNIFORM1IPROC, glUniform1i) // Core in v2.0
EXTERNOGLEXT(PFNGLUNIFORM1IVPROC, glUniform1iv) // Core in v2.0
EXTERNOGLEXT(PFNGLUNIFORM1FPROC, glUniform1f) // Core in v2.0
EXTERNOGLEXT(PFNGLUNIFORM1FVPROC, glUniform1fv) // Core in v2.0
EXTERNOGLEXT(PFNGLUNIFORM2FPROC, glUniform2f) // Core in v2.0
EXTERNOGLEXT(PFNGLUNIFORM4FPROC, glUniform4f) // Core in v2.0
EXTERNOGLEXT(PFNGLUNIFORM4FVPROC, glUniform4fv) // Core in v2.0
EXTERNOGLEXT(PFNGLDRAWBUFFERSPROC, glDrawBuffers) // Core in v2.0

// Generic vertex attributes
EXTERNOGLEXT(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) // Core in v2.0
EXTERNOGLEXT(PFNGLBINDFRAGDATALOCATIONPROC, glBindFragDataLocation) // Core in v3.0
EXTERNOGLEXT(PFNGLBINDFRAGDATALOCATIONINDEXEDPROC, glBindFragDataLocationIndexed) // Core in v3.3
EXTERNOGLEXT(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) // Core in v2.0
EXTERNOGLEXT(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) // Core in v2.0
EXTERNOGLEXT(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) // Core in v2.0

// VAO
EXTERNOGLEXT(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) // Core in v3.0
EXTERNOGLEXT(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays) // Core in v3.0
EXTERNOGLEXT(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) // Core in v3.0

// VBO and PBO
EXTERNOGLEXT(PFNGLGENBUFFERSPROC, glGenBuffers) // Core in v1.5
EXTERNOGLEXT(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) // Core in v1.5
EXTERNOGLEXT(PFNGLBINDBUFFERPROC, glBindBuffer) // Core in v1.5
EXTERNOGLEXT(PFNGLBUFFERDATAPROC, glBufferData) // Core in v1.5
EXTERNOGLEXT(PFNGLBUFFERSUBDATAPROC, glBufferSubData) // Core in v1.5
EXTERNOGLEXT(PFNGLMAPBUFFERPROC, glMapBuffer) // Core in v1.5
EXTERNOGLEXT(PFNGLUNMAPBUFFERPROC, glUnmapBuffer) // Core in v1.5

// Buffer Objects
EXTERNOGLEXT(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange) // Core in v3.0

// FBO
EXTERNOGLEXT(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) // Core in v3.0
EXTERNOGLEXT(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) // Core in v3.0
EXTERNOGLEXT(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) // Core in v3.0
EXTERNOGLEXT(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) // Core in v3.0
EXTERNOGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) // Core in v3.0
EXTERNOGLEXT(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) // Core in v3.0
EXTERNOGLEXT(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer) // Core in v3.0
EXTERNOGLEXT(PFNGLDRAWBUFFERSPROC, glDrawBuffers) // Core in v2.0

// Multisampled FBO
EXTERNOGLEXT(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) // Core in v3.0
EXTERNOGLEXT(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) // Core in v3.0
EXTERNOGLEXT(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) // Core in v3.0
EXTERNOGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, glRenderbufferStorageMultisample) // Core in v3.0
EXTERNOGLEXT(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers) // Core in v3.0
EXTERNOGLEXT(PFNGLTEXIMAGE2DMULTISAMPLEPROC, glTexImage2DMultisample) // Core in v3.2

// UBO
EXTERNOGLEXT(PFNGLGETUNIFORMBLOCKINDEXPROC, glGetUniformBlockIndex) // Core in v3.1
EXTERNOGLEXT(PFNGLUNIFORMBLOCKBINDINGPROC, glUniformBlockBinding) // Core in v3.1
EXTERNOGLEXT(PFNGLBINDBUFFERBASEPROC, glBindBufferBase) // Core in v3.0
EXTERNOGLEXT(PFNGLGETACTIVEUNIFORMBLOCKIVPROC, glGetActiveUniformBlockiv) // Core in v3.1

// TBO
EXTERNOGLEXT(PFNGLTEXBUFFERPROC, glTexBuffer) // Core in v3.1

// Sync Objects
EXTERNOGLEXT(PFNGLFENCESYNCPROC, glFenceSync) // Core in v3.2
EXTERNOGLEXT(PFNGLWAITSYNCPROC, glWaitSync) // Core in v3.2
EXTERNOGLEXT(PFNGLDELETESYNCPROC, glDeleteSync) // Core in v3.2

#endif // OGLRENDER_3_2_H

// Define the minimum required OpenGL version for the driver to support
#define OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MAJOR			1
#define OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MINOR			2
#define OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_REVISION		0

#define OGLRENDER_VERT_INDEX_BUFFER_COUNT	(POLYLIST_SIZE * 6)

// Assign the FBO attachments for the main geometry render
#ifdef OGLRENDER_3_2_H
	#define GL_COLOROUT_ATTACHMENT_ID			GL_COLOR_ATTACHMENT0
	#define GL_WORKING_ATTACHMENT_ID			GL_COLOR_ATTACHMENT3
	#define GL_POLYID_ATTACHMENT_ID				GL_COLOR_ATTACHMENT1
	#define GL_FOGATTRIBUTES_ATTACHMENT_ID		GL_COLOR_ATTACHMENT2
#else
	#define GL_COLOROUT_ATTACHMENT_ID			GL_COLOR_ATTACHMENT0_EXT
	#define GL_WORKING_ATTACHMENT_ID			GL_COLOR_ATTACHMENT3_EXT
	#define GL_POLYID_ATTACHMENT_ID				GL_COLOR_ATTACHMENT1_EXT
	#define GL_FOGATTRIBUTES_ATTACHMENT_ID		GL_COLOR_ATTACHMENT2_EXT
#endif

enum OGLVertexAttributeID
{
	OGLVertexAttributeID_Position	= 0,
	OGLVertexAttributeID_TexCoord0	= 8,
	OGLVertexAttributeID_Color		= 3
};

enum OGLTextureUnitID
{
	// Main textures will always be on texture unit 0.
	OGLTextureUnitID_FinalColor = 1,
	OGLTextureUnitID_GColor,
	OGLTextureUnitID_DepthStencil,
	OGLTextureUnitID_GPolyID,
	OGLTextureUnitID_FogAttr,
	OGLTextureUnitID_PolyStates,
	OGLTextureUnitID_FogDensityTable
};

enum OGLBindingPointID
{
	OGLBindingPointID_RenderStates = 0,
	OGLBindingPointID_PolyStates = 1
};

enum OGLErrorCode
{
	OGLERROR_NOERR = RENDER3DERROR_NOERR,
	
	OGLERROR_DRIVER_VERSION_TOO_OLD,
	
	OGLERROR_BEGINGL_FAILED,
	OGLERROR_CLIENT_RESIZE_ERROR,
	
	OGLERROR_FEATURE_UNSUPPORTED,
	OGLERROR_VBO_UNSUPPORTED,
	OGLERROR_PBO_UNSUPPORTED,
	OGLERROR_SHADER_UNSUPPORTED,
	OGLERROR_VAO_UNSUPPORTED,
	OGLERROR_FBO_UNSUPPORTED,
	OGLERROR_MULTISAMPLED_FBO_UNSUPPORTED,
	
	OGLERROR_VERTEX_SHADER_PROGRAM_LOAD_ERROR,
	OGLERROR_FRAGMENT_SHADER_PROGRAM_LOAD_ERROR,
	OGLERROR_SHADER_CREATE_ERROR,
	
	OGLERROR_FBO_CREATE_ERROR
};

enum OGLPolyDrawMode
{
	OGLPolyDrawMode_DrawOpaquePolys			= 0,
	OGLPolyDrawMode_DrawTranslucentPolys	= 1,
	OGLPolyDrawMode_ZeroAlphaPass			= 2
};

union GLvec2
{
	struct { GLfloat x, y; };
	GLfloat v[2];
};

union GLvec3
{
	struct { GLfloat r, g, b; };
	struct { GLfloat x, y, z; };
	GLfloat v[3];
};

union GLvec4
{
	struct { GLfloat r, g, b, a; };
	struct { GLfloat x, y, z, w; };
	GLfloat v[4];
};

struct OGLVertex
{
	GLvec4 position;
	GLvec2 texCoord;
	GLvec3 color;
};

struct OGLRenderStates
{
	GLuint enableAntialiasing;
	GLuint enableFogAlphaOnly;
	GLuint clearPolyID;
	GLfloat clearDepth;
	GLfloat alphaTestRef;
	GLfloat fogOffset; // Currently unused, kept to preserve alignment
	GLfloat fogStep; // Currently unused, kept to preserve alignment
	GLfloat pad_0; // This needs to be here to preserve alignment
	GLvec4 fogColor;
	GLvec4 edgeColor[8];
	GLvec4 toonColor[32];
};

union OGLPolyStates
{
	u32 packedState;
	
	struct
	{
		u8 PolygonID:6;
		u8 PolygonMode:2;
		
		u8 PolygonAlpha:5;
		u8 IsWireframe:1;
		u8 EnableFog:1;
		u8 SetNewDepthForTranslucent:1;
		
		u8 EnableTexture:1;
		u8 TexSingleBitAlpha:1;
		u8 TexSizeShiftS:3;
		u8 TexSizeShiftT:3;
		
		u8 :8;
	};
};

union OGLGeometryFlags
{
	u8 value;
	
#ifndef MSB_FIRST
	struct
	{
		u8 EnableFog:1;
		u8 EnableEdgeMark:1;
		u8 OpaqueDrawMode:1;
		u8 EnableWDepth:1;
		u8 EnableAlphaTest:1;
		u8 EnableTextureSampling:1;
		u8 ToonShadingMode:1;
		u8 unused:1;
	};
	
	struct
	{
		u8 DrawBuffersMode:3;
		u8 :5;
	};
#else
	struct
	{
		u8 unused:1;
		u8 ToonShadingMode:1;
		u8 EnableTextureSampling:1;
		u8 EnableAlphaTest:1;
		u8 EnableWDepth:1;
		u8 OpaqueDrawMode:1;
		u8 EnableEdgeMark:1;
		u8 EnableFog:1;
	};
	
	struct
	{
		u8 :5;
		u8 DrawBuffersMode:3;
	};
#endif
};
typedef OGLGeometryFlags OGLGeometryFlags;

union OGLFogProgramKey
{
	u32 key;
	
	struct
	{
		u16 offset;
		u8 shift;
		u8 :8;
	};
};
typedef OGLFogProgramKey OGLFogProgramKey;

struct OGLFogShaderID
{
	GLuint program;
	GLuint fragShader;
};
typedef OGLFogShaderID OGLFogShaderID;

struct OGLRenderRef
{	
	// OpenGL Feature Support
	GLint stateTexMirroredRepeat;
	
	// VBO
	GLuint vboGeometryVtxID;
	GLuint iboGeometryIndexID;
	GLuint vboPostprocessVtxID;
	
	// PBO
	GLuint pboRenderDataID;
	
	// UBO / TBO
	GLuint uboRenderStatesID;
	GLuint uboPolyStatesID;
	GLuint tboPolyStatesID;
	GLuint texPolyStatesID;
	
	// FBO
	GLuint texCIColorID;
	GLuint texCIFogAttrID;
	GLuint texCIDepthStencilID;
	
	GLuint texGColorID;
	GLuint texGFogAttrID;
	GLuint texGPolyID;
	GLuint texGDepthStencilID;
	GLuint texFinalColorID;
	GLuint texFogDensityTableID;
	GLuint texMSGColorID;
	GLuint texMSGWorkingID;
	
	GLuint rboMSGColorID;
	GLuint rboMSGWorkingID;
	GLuint rboMSGPolyID;
	GLuint rboMSGFogAttrID;
	GLuint rboMSGDepthStencilID;
	
	GLuint fboClearImageID;
	GLuint fboRenderID;
	GLuint fboMSIntermediateRenderID;
	GLuint selectedRenderingFBO;
	
	// Shader states
	GLuint vertexGeometryShaderID;
	GLuint fragmentGeometryShaderID[128];
	GLuint programGeometryID[128];
	
	GLuint vtxShaderGeometryZeroDstAlphaID;
	GLuint fragShaderGeometryZeroDstAlphaID;
	GLuint programGeometryZeroDstAlphaID;
	
	GLuint vtxShaderMSGeometryZeroDstAlphaID;
	GLuint fragShaderMSGeometryZeroDstAlphaID;
	GLuint programMSGeometryZeroDstAlphaID;
	
	GLuint vertexEdgeMarkShaderID;
	GLuint vertexFogShaderID;
	GLuint vertexFramebufferOutput6665ShaderID;
	GLuint vertexFramebufferOutput8888ShaderID;
	GLuint fragmentEdgeMarkShaderID;
	GLuint fragmentFramebufferRGBA6665OutputShaderID;
	GLuint fragmentFramebufferRGBA8888OutputShaderID;
	GLuint programEdgeMarkID;
	GLuint programFramebufferRGBA6665OutputID[2];
	GLuint programFramebufferRGBA8888OutputID[2];
	
	GLint uniformStateEnableFogAlphaOnly;
	GLint uniformStateClearPolyID;
	GLint uniformStateClearDepth;
	GLint uniformStateEdgeColor;
	GLint uniformStateFogColor;
	
	GLint uniformStateAlphaTestRef[256];
	GLint uniformStateToonColor[256];
	GLint uniformPolyTexScale[256];
	GLint uniformPolyMode[256];
	GLint uniformPolyIsWireframe[256];
	GLint uniformPolySetNewDepthForTranslucent[256];
	GLint uniformPolyAlpha[256];
	GLint uniformPolyID[256];
	
	GLint uniformPolyEnableTexture[256];
	GLint uniformPolyEnableFog[256];
	GLint uniformTexSingleBitAlpha[256];
	GLint uniformTexDrawOpaque[256];
	GLint uniformDrawModeDepthEqualsTest[256];
	
	GLint uniformPolyStateIndex[256];
	GLfloat uniformPolyDepthOffset[256];
	GLint uniformPolyDrawShadow[256];
	
	// VAO
	GLuint vaoGeometryStatesID;
	GLuint vaoPostprocessStatesID;
	
	// Client-side Buffers
	GLfloat *color4fBuffer;
	CACHE_ALIGN GLushort vertIndexBuffer[OGLRENDER_VERT_INDEX_BUFFER_COUNT];
	CACHE_ALIGN GLushort workingCIColorBuffer[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	CACHE_ALIGN GLuint workingCIDepthStencilBuffer[2][GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	CACHE_ALIGN GLuint workingCIFogAttributesBuffer[2][GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	
	// Vertex Attributes Pointers
	GLvoid *vtxPtrPosition;
	GLvoid *vtxPtrTexCoord;
	GLvoid *vtxPtrColor;
};

struct GFX3D_State;
struct POLYLIST;
struct INDEXLIST;
struct POLY;
class OpenGLRenderer;

extern GPU3DInterface gpu3Dgl;
extern GPU3DInterface gpu3DglOld;
extern GPU3DInterface gpu3Dgl_3_2;

extern const GLenum GeometryDrawBuffersEnum[8][4];
extern const GLint GeometryAttachmentWorkingBuffer[8];
extern const GLint GeometryAttachmentPolyID[8];
extern const GLint GeometryAttachmentFogAttributes[8];

extern CACHE_ALIGN const GLfloat divide5bitBy31_LUT[32];
extern CACHE_ALIGN const GLfloat divide6bitBy63_LUT[64];
extern const GLfloat PostprocessVtxBuffer[16];
extern const GLubyte PostprocessElementBuffer[6];

//This is called by OGLRender whenever it initializes.
//Platforms, please be sure to set this up.
//return true if you successfully init.
extern bool (*oglrender_init)();

//This is called by OGLRender before it uses opengl.
//return true if youre OK with using opengl
extern bool (*oglrender_beginOpenGL)();

//This is called by OGLRender after it is done using opengl.
extern void (*oglrender_endOpenGL)();

//This is called by OGLRender whenever the framebuffer is resized.
extern bool (*oglrender_framebufferDidResizeCallback)(const bool isFBOSupported, size_t w, size_t h);

// Helper functions for calling the above function pointers at the
// beginning and ending of OpenGL commands.
bool BEGINGL();
void ENDGL();

// These functions need to be assigned by ports that support using an
// OpenGL 3.2 Core Profile context. The OGLRender_3_2.cpp file includes
// the corresponding functions to assign to each function pointer.
//
// If any of these functions are unassigned, then one of the legacy OpenGL
// renderers will be used instead.
extern void (*OGLLoadEntryPoints_3_2_Func)();
extern void (*OGLCreateRenderer_3_2_Func)(OpenGLRenderer **rendererPtr);

bool IsOpenGLDriverVersionSupported(unsigned int checkVersionMajor, unsigned int checkVersionMinor, unsigned int checkVersionRevision);

class OpenGLTexture : public Render3DTexture
{
protected:
	GLuint _texID;
	GLfloat _invSizeS;
	GLfloat _invSizeT;
	bool _isTexInited;
	
	u32 *_upscaleBuffer;
	
public:
	OpenGLTexture(TEXIMAGE_PARAM texAttributes, u32 palAttributes);
	virtual ~OpenGLTexture();
	
	virtual void Load(bool forceTextureInit);
	
	GLuint GetID() const;
	GLfloat GetInvWidth() const;
	GLfloat GetInvHeight() const;
	
	void SetUnpackBuffer(void *unpackBuffer);
	void SetDeposterizeBuffer(void *dstBuffer, void *workingBuffer);
	void SetUpscalingBuffer(void *upscaleBuffer);
};

#if defined(ENABLE_AVX2)
class OpenGLRenderer : public Render3D_AVX2
#elif defined(ENABLE_SSE2)
class OpenGLRenderer : public Render3D_SSE2
#elif defined(ENABLE_ALTIVEC)
class OpenGLRenderer : public Render3D_AltiVec
#else
class OpenGLRenderer : public Render3D
#endif
{
private:
	// Driver's OpenGL Version
	unsigned int versionMajor;
	unsigned int versionMinor;
	unsigned int versionRevision;
	
private:
	Render3DError _FlushFramebufferFlipAndConvertOnCPU(const FragmentColor *__restrict srcFramebuffer,
													   FragmentColor *__restrict dstFramebufferMain, u16 *__restrict dstFramebuffer16,
													   bool doFramebufferFlip, bool doFramebufferConvert);
	
protected:
	// OpenGL-specific References
	OGLRenderRef *ref;
	
	// OpenGL Feature Support
	bool isVBOSupported;
	bool isPBOSupported;
	bool isFBOSupported;
	bool isMultisampledFBOSupported;
	bool isShaderSupported;
	bool isVAOSupported;
	bool willFlipOnlyFramebufferOnGPU;
	bool willFlipAndConvertFramebufferOnGPU;
	bool willUsePerSampleZeroDstPass;
	
	bool _emulateShadowPolygon;
	bool _emulateSpecialZeroAlphaBlending;
	bool _emulateNDSDepthCalculation;
	bool _emulateDepthLEqualPolygonFacing;
	
	FragmentColor *_mappedFramebuffer;
	FragmentColor *_workingTextureUnpackBuffer;
	bool _pixelReadNeedsFinish;
	bool _needsZeroDstAlphaPass;
	size_t _currentPolyIndex;
	bool _enableAlphaBlending;
	OGLTextureUnitID _lastTextureDrawTarget;
	OGLGeometryFlags _geometryProgramFlags;
	OGLFogProgramKey _fogProgramKey;
	std::map<u32, OGLFogShaderID> _fogProgramMap;
	
	CACHE_ALIGN OGLRenderStates _pendingRenderStates;
	
	bool _enableMultisampledRendering;
	int _selectedMultisampleSize;
	bool _isPolyFrontFacing[POLYLIST_SIZE];
	size_t _clearImageIndex;
	
	Render3DError FlushFramebuffer(const FragmentColor *__restrict srcFramebuffer, FragmentColor *__restrict dstFramebufferMain, u16 *__restrict dstFramebuffer16);
	OpenGLTexture* GetLoadedTextureFromPolygon(const POLY &thePoly, bool enableTexturing);
	
	template<OGLPolyDrawMode DRAWMODE> size_t DrawPolygonsForIndexRange(const CPoly *clippedPolyList, const size_t clippedPolyCount, size_t firstIndex, size_t lastIndex, size_t &indexOffset, POLYGON_ATTR &lastPolyAttr);
	template<OGLPolyDrawMode DRAWMODE> Render3DError DrawAlphaTexturePolygon(const GLenum polyPrimitive,
																			 const GLsizei vertIndexCount,
																			 const GLushort *indexBufferPtr,
																			 const bool performDepthEqualTest,
																			 const bool enableAlphaDepthWrite,
																			 const bool canHaveOpaqueFragments,
																			 const u8 opaquePolyID,
																			 const bool isPolyFrontFacing);
	template<OGLPolyDrawMode DRAWMODE> Render3DError DrawOtherPolygon(const GLenum polyPrimitive,
																	  const GLsizei vertIndexCount,
																	  const GLushort *indexBufferPtr,
																	  const bool performDepthEqualTest,
																	  const bool enableAlphaDepthWrite,
																	  const u8 opaquePolyID,
																	  const bool isPolyFrontFacing);
	
	// OpenGL-specific methods
	virtual Render3DError CreateVBOs() = 0;
	virtual void DestroyVBOs() = 0;
	virtual Render3DError CreatePBOs() = 0;
	virtual void DestroyPBOs() = 0;
	virtual Render3DError CreateFBOs() = 0;
	virtual void DestroyFBOs() = 0;
	virtual Render3DError CreateMultisampledFBO(GLsizei numSamples) = 0;
	virtual void DestroyMultisampledFBO() = 0;
	virtual void ResizeMultisampledFBOs(GLsizei numSamples) = 0;
	virtual Render3DError CreateVAOs() = 0;
	virtual void DestroyVAOs() = 0;
	
	virtual Render3DError CreateGeometryPrograms() = 0;
	virtual void DestroyGeometryPrograms() = 0;
	virtual Render3DError CreateGeometryZeroDstAlphaProgram(const char *vtxShaderCString, const char *fragShaderCString) = 0;
	virtual void DestroyGeometryZeroDstAlphaProgram() = 0;
	virtual Render3DError CreateEdgeMarkProgram(const char *vtxShaderCString, const char *fragShaderCString) = 0;
	virtual void DestroyEdgeMarkProgram() = 0;
	virtual Render3DError CreateFogProgram(const OGLFogProgramKey fogProgramKey, const char *vtxShaderCString, const char *fragShaderCString) = 0;
	virtual void DestroyFogProgram(const OGLFogProgramKey fogProgramKey) = 0;
	virtual void DestroyFogPrograms() = 0;
	virtual Render3DError CreateFramebufferOutput6665Program(const size_t outColorIndex, const char *vtxShaderCString, const char *fragShaderCString) = 0;
	virtual void DestroyFramebufferOutput6665Programs() = 0;
	virtual Render3DError CreateFramebufferOutput8888Program(const size_t outColorIndex, const char *vtxShaderCString, const char *fragShaderCString) = 0;
	virtual void DestroyFramebufferOutput8888Programs() = 0;
	
	virtual Render3DError InitFinalRenderStates(const std::set<std::string> *oglExtensionSet) = 0;
	virtual Render3DError InitPostprocessingPrograms(const char *edgeMarkVtxShader,
													 const char *edgeMarkFragShader,
													 const char *framebufferOutputVtxShader,
													 const char *framebufferOutputRGBA6665FragShader,
													 const char *framebufferOutputRGBA8888FragShader) = 0;
	
	virtual Render3DError UploadClearImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID) = 0;
	
	virtual void GetExtensionSet(std::set<std::string> *oglExtensionSet) = 0;
	virtual void _SetupGeometryShaders(const OGLGeometryFlags flags) = 0;
	virtual Render3DError EnableVertexAttributes() = 0;
	virtual Render3DError DisableVertexAttributes() = 0;
	virtual void _ResolveWorkingBackFacing() = 0;
	virtual void _ResolveGeometry() = 0;
	virtual Render3DError ReadBackPixels() = 0;
	
	virtual Render3DError DrawShadowPolygon(const GLenum polyPrimitive, const GLsizei vertIndexCount, const GLushort *indexBufferPtr, const bool performDepthEqualTest, const bool enableAlphaDepthWrite, const bool isTranslucent, const u8 opaquePolyID) = 0;
	virtual void SetPolygonIndex(const size_t index) = 0;
	virtual Render3DError SetupPolygon(const POLY &thePoly, bool treatAsTranslucent, bool willChangeStencilBuffer) = 0;
	
public:
	OpenGLRenderer();
	virtual ~OpenGLRenderer();
	
	virtual Render3DError InitExtensions() = 0;
	
	bool IsExtensionPresent(const std::set<std::string> *oglExtensionSet, const std::string extensionName) const;
	Render3DError ShaderProgramCreate(GLuint &vtxShaderID,
									  GLuint &fragShaderID,
									  GLuint &programID,
									  const char *vtxShaderCString,
									  const char *fragShaderCString);
	bool ValidateShaderCompile(GLenum shaderType, GLuint theShader) const;
	bool ValidateShaderProgramLink(GLuint theProgram) const;
	void GetVersion(unsigned int *major, unsigned int *minor, unsigned int *revision) const;
	void SetVersion(unsigned int major, unsigned int minor, unsigned int revision);
	bool IsVersionSupported(unsigned int checkVersionMajor, unsigned int checkVersionMinor, unsigned int checkVersionRevision) const;
	
	virtual FragmentColor* GetFramebuffer();
	virtual GLsizei GetLimitedMultisampleSize() const;
	
	Render3DError ApplyRenderingSettings(const GFX3D_State &renderState);
};

class OpenGLRenderer_1_2 : public OpenGLRenderer
{
protected:
	// OpenGL-specific methods
	virtual Render3DError CreateVBOs();
	virtual void DestroyVBOs();
	virtual Render3DError CreatePBOs();
	virtual void DestroyPBOs();
	virtual Render3DError CreateFBOs();
	virtual void DestroyFBOs();
	virtual Render3DError CreateMultisampledFBO(GLsizei numSamples);
	virtual void DestroyMultisampledFBO();
	virtual void ResizeMultisampledFBOs(GLsizei numSamples);
	virtual Render3DError CreateVAOs();
	virtual void DestroyVAOs();
	
	virtual Render3DError CreateGeometryPrograms();
	virtual void DestroyGeometryPrograms();
	virtual Render3DError CreateGeometryZeroDstAlphaProgram(const char *vtxShaderCString, const char *fragShaderCString);
	virtual void DestroyGeometryZeroDstAlphaProgram();
	virtual Render3DError CreateEdgeMarkProgram(const char *vtxShaderCString, const char *fragShaderCString);
	virtual void DestroyEdgeMarkProgram();
	virtual Render3DError CreateFogProgram(const OGLFogProgramKey fogProgramKey, const char *vtxShaderCString, const char *fragShaderCString);
	virtual void DestroyFogProgram(const OGLFogProgramKey fogProgramKey);
	virtual void DestroyFogPrograms();
	virtual Render3DError CreateFramebufferOutput6665Program(const size_t outColorIndex, const char *vtxShaderCString, const char *fragShaderCString);
	virtual void DestroyFramebufferOutput6665Programs();
	virtual Render3DError CreateFramebufferOutput8888Program(const size_t outColorIndex, const char *vtxShaderCString, const char *fragShaderCString);
	virtual void DestroyFramebufferOutput8888Programs();
	
	virtual Render3DError InitFinalRenderStates(const std::set<std::string> *oglExtensionSet);
	virtual Render3DError InitPostprocessingPrograms(const char *edgeMarkVtxShader,
													 const char *edgeMarkFragShader,
													 const char *framebufferOutputVtxShader,
													 const char *framebufferOutputRGBA6665FragShader,
													 const char *framebufferOutputRGBA8888FragShader);
	
	virtual Render3DError UploadClearImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID);
	
	virtual void GetExtensionSet(std::set<std::string> *oglExtensionSet);
	virtual void _SetupGeometryShaders(const OGLGeometryFlags flags);
	virtual Render3DError EnableVertexAttributes();
	virtual Render3DError DisableVertexAttributes();
	virtual Render3DError ZeroDstAlphaPass(const CPoly *clippedPolyList, const size_t clippedPolyCount, bool enableAlphaBlending, size_t indexOffset, POLYGON_ATTR lastPolyAttr);
	virtual void _ResolveWorkingBackFacing();
	virtual void _ResolveGeometry();
	virtual Render3DError ReadBackPixels();
	
	// Base rendering methods
	virtual Render3DError BeginRender(const GFX3D &engine);
	virtual Render3DError RenderGeometry();
	virtual Render3DError PostprocessFramebuffer();
	virtual Render3DError EndRender();
	
	virtual Render3DError ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID);
	virtual Render3DError ClearUsingValues(const FragmentColor &clearColor6665, const FragmentAttributes &clearAttributes);
	
	virtual void SetPolygonIndex(const size_t index);
	virtual Render3DError SetupPolygon(const POLY &thePoly, bool treatAsTranslucent, bool willChangeStencilBuffer);
	virtual Render3DError SetupTexture(const POLY &thePoly, size_t polyRenderIndex);
	virtual Render3DError SetupViewport(const u32 viewportValue);
	
	virtual Render3DError DrawShadowPolygon(const GLenum polyPrimitive, const GLsizei vertIndexCount, const GLushort *indexBufferPtr, const bool performDepthEqualTest, const bool enableAlphaDepthWrite, const bool isTranslucent, const u8 opaquePolyID);
	
public:
	~OpenGLRenderer_1_2();
	
	virtual Render3DError InitExtensions();
	virtual Render3DError Reset();
	virtual Render3DError RenderPowerOff();
	virtual Render3DError RenderFinish();
	virtual Render3DError RenderFlush(bool willFlushBuffer32, bool willFlushBuffer16);
	virtual Render3DError SetFramebufferSize(size_t w, size_t h);
};

class OpenGLRenderer_2_0 : public OpenGLRenderer_1_2
{
protected:
	virtual Render3DError InitFinalRenderStates(const std::set<std::string> *oglExtensionSet);
	
	virtual Render3DError EnableVertexAttributes();
	virtual Render3DError DisableVertexAttributes();
	
	virtual Render3DError BeginRender(const GFX3D &engine);
	
	virtual Render3DError SetupTexture(const POLY &thePoly, size_t polyRenderIndex);
};

class OpenGLRenderer_2_1 : public OpenGLRenderer_2_0
{	
public:
	virtual Render3DError RenderFinish();
	virtual Render3DError RenderFlush(bool willFlushBuffer32, bool willFlushBuffer16);
};

#endif
