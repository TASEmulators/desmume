/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2018 DeSmuME team

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

#endif // OGLRENDER_3_2_H

// Define the minimum required OpenGL version for the driver to support
#define OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MAJOR			1
#define OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MINOR			2
#define OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_REVISION		0

#define OGLRENDER_VERT_INDEX_BUFFER_COUNT	(POLYLIST_SIZE * 6)

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
	OGLTextureUnitID_ToonTable,
	OGLTextureUnitID_GColor,
	OGLTextureUnitID_DepthStencil,
	OGLTextureUnitID_GPolyID,
	OGLTextureUnitID_FogAttr,
	OGLTextureUnitID_PolyStates
};

enum OGLBindingPointID
{
	OGLBindingPointID_RenderStates = 0
};

enum OGLErrorCode
{
	OGLERROR_NOERR = RENDER3DERROR_NOERR,
	
	OGLERROR_DRIVER_VERSION_TOO_OLD,
	
	OGLERROR_BEGINGL_FAILED,
	
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
	GLvec2 framebufferSize;
	GLint toonShadingMode;
	GLuint enableAlphaTest;
	GLuint enableAntialiasing;
	GLuint enableEdgeMarking;
	GLuint enableFogAlphaOnly;
	GLuint useWDepth;
	GLuint clearPolyID;
	GLfloat clearDepth;
	GLfloat alphaTestRef;
	GLfloat fogOffset;
	GLfloat fogStep;
	GLfloat pad[3]; // This needs to be here to preserve alignment
	GLvec4 fogColor;
	GLvec4 fogDensity[32]; // Array of floats need to be padded as vec4
	GLvec4 edgeColor[8];
	GLvec4 toonColor[32];
};

struct OGLPolyStates
{
	union
	{
		struct { GLubyte enableTexture, enableFog, isWireframe, setNewDepthForTranslucent; };
		GLubyte flags[4];
	};
	
	union
	{
		struct { GLubyte polyAlpha, polyMode, polyID, valuesPad[1]; };
		GLubyte values[4];
	};
	
	union
	{
		struct { GLubyte texSizeS, texSizeT, texSingleBitAlpha, texParamPad[1]; };
		GLubyte texParam[4];
	};
};

struct OGLRenderRef
{	
	// OpenGL Feature Support
	GLint stateTexMirroredRepeat;
	
	// VBO
	GLuint vboGeometryVtxID;
	GLuint iboGeometryIndexID;
	GLuint vboPostprocessVtxID;
	GLuint iboPostprocessIndexID;
	
	// PBO
	GLuint pboRenderDataID;
	
	// UBO / TBO
	GLuint uboRenderStatesID;
	GLuint tboPolyStatesID;
	GLuint texPolyStatesID;
	
	// FBO
	GLuint texCIColorID;
	GLuint texCIFogAttrID;
	GLuint texCIPolyID;
	GLuint texCIDepthStencilID;
	
	GLuint texGColorID;
	GLuint texGFogAttrID;
	GLuint texGPolyID;
	GLuint texGDepthStencilID;
	GLuint texFinalColorID;
	GLuint texMSGColorID;
	
	GLuint rboMSGColorID;
	GLuint rboMSGPolyID;
	GLuint rboMSGFogAttrID;
	GLuint rboMSGDepthStencilID;
	
	GLuint fboClearImageID;
	GLuint fboRenderID;
	GLuint fboPostprocessID;
	GLuint fboMSIntermediateRenderID;
	GLuint selectedRenderingFBO;
	
	// Shader states
	GLuint vertexGeometryShaderID;
	GLuint fragmentGeometryShaderID;
	GLuint programGeometryID;
	
	GLuint vtxShaderGeometryZeroDstAlphaID;
	GLuint fragShaderGeometryZeroDstAlphaID;
	GLuint programGeometryZeroDstAlphaID;
	
	GLuint vtxShaderMSGeometryZeroDstAlphaID;
	GLuint fragShaderMSGeometryZeroDstAlphaID;
	GLuint programMSGeometryZeroDstAlphaID;
	
	GLuint vertexEdgeMarkShaderID;
	GLuint vertexFogShaderID;
	GLuint vertexFramebufferOutputShaderID;
	GLuint fragmentEdgeMarkShaderID;
	GLuint fragmentFogShaderID;
	GLuint fragmentFramebufferRGBA6665OutputShaderID;
	GLuint fragmentFramebufferRGBA8888OutputShaderID;
	GLuint programEdgeMarkID;
	GLuint programFogID;
	GLuint programFramebufferRGBA6665OutputID;
	GLuint programFramebufferRGBA8888OutputID;
	
	GLint uniformFramebufferSize_ConvertRGBA6665;
	GLint uniformFramebufferSize_ConvertRGBA8888;
	GLint uniformTexInFragColor_ConvertRGBA6665;
	GLint uniformTexInFragColor_ConvertRGBA8888;
	GLint uniformStateToonShadingMode;
	GLint uniformStateEnableAlphaTest;
	GLint uniformStateEnableAntialiasing;
	GLint uniformStateEnableEdgeMarking;
	GLint uniformStateEnableFogAlphaOnly;
	GLint uniformStateUseWDepth;
	GLint uniformStateAlphaTestRef;
	GLint uniformFramebufferSize_EdgeMark;
	GLint uniformStateClearPolyID;
	GLint uniformStateClearDepth;
	GLint uniformStateEdgeColor;
	GLint uniformStateFogColor;
	GLint uniformStateFogDensity;
	GLint uniformStateFogOffset;
	GLint uniformStateFogStep;
	
	GLint uniformPolyTexScale;
	GLint uniformPolyMode;
	GLint uniformPolyIsWireframe;
	GLint uniformPolySetNewDepthForTranslucent;
	GLint uniformPolyAlpha;
	GLint uniformPolyID;
	
	GLint uniformPolyEnableTexture;
	GLint uniformPolyEnableFog;
	GLint uniformTexSingleBitAlpha;
	GLint uniformTexDrawOpaque;
	
	GLint uniformPolyStateIndex;
	GLint uniformPolyDepthOffsetMode;
	GLint uniformPolyDrawShadow;
	
	GLuint texToonTableID;
	
	// VAO
	GLuint vaoGeometryStatesID;
	GLuint vaoPostprocessStatesID;
	
	// Client-side Buffers
	GLfloat *color4fBuffer;
	GLushort *vertIndexBuffer;
	CACHE_ALIGN GLushort workingCIColorBuffer[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	CACHE_ALIGN GLuint workingCIDepthStencilBuffer[2][GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	CACHE_ALIGN GLuint workingCIFogAttributesBuffer[2][GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	CACHE_ALIGN GLuint workingCIPolyIDBuffer[2][GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	
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

extern const GLenum RenderDrawList[3];
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
extern bool (*oglrender_framebufferDidResizeCallback)(size_t w, size_t h);

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

bool IsVersionSupported(unsigned int checkVersionMajor, unsigned int checkVersionMinor, unsigned int checkVersionRevision);

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
class OpenGLRenderer : public Render3D_Altivec
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
	bool isSampleShadingSupported;
	bool willFlipOnlyFramebufferOnGPU;
	bool willFlipAndConvertFramebufferOnGPU;
	bool willUsePerSampleZeroDstPass;
	
	bool _emulateShadowPolygon;
	bool _emulateSpecialZeroAlphaBlending;
	bool _emulateDepthEqualsTestTolerance;
	bool _emulateDepthLEqualPolygonFacing;
	
	float _clearDepth;
	
	
	FragmentColor *_mappedFramebuffer;
	FragmentColor *_workingTextureUnpackBuffer;
	bool _pixelReadNeedsFinish;
	bool _needsZeroDstAlphaPass;
	size_t _currentPolyIndex;
	OGLTextureUnitID _lastTextureDrawTarget;
	
	bool _enableMultisampledRendering;
	int _selectedMultisampleSize;
	bool _isPolyFrontFacing[POLYLIST_SIZE];
	size_t _clearImageIndex;
	
	Render3DError FlushFramebuffer(const FragmentColor *__restrict srcFramebuffer, FragmentColor *__restrict dstFramebufferMain, u16 *__restrict dstFramebuffer16);
	OpenGLTexture* GetLoadedTextureFromPolygon(const POLY &thePoly, bool enableTexturing);
	template<OGLPolyDrawMode DRAWMODE> size_t DrawPolygonsForIndexRange(const POLYLIST *polyList, const INDEXLIST *indexList, size_t firstIndex, size_t lastIndex, size_t &indexOffset, POLYGON_ATTR &lastPolyAttr);
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
	virtual Render3DError InitGeometryProgram(const char *geometryVtxShaderCString, const char *geometryFragShaderCString,
											  const char *geometryAlphaVtxShaderCString, const char *geometryAlphaFragShaderCString,
											  const char *geometryMSAlphaVtxShaderCString, const char *geometryMSAlphaFragShaderCString) = 0;
	virtual void DestroyGeometryProgram() = 0;
	virtual Render3DError CreateVAOs() = 0;
	virtual void DestroyVAOs() = 0;
	virtual Render3DError InitFinalRenderStates(const std::set<std::string> *oglExtensionSet) = 0;
	virtual Render3DError InitTables() = 0;
	virtual Render3DError InitPostprocessingPrograms(const char *edgeMarkVtxShader,
													 const char *edgeMarkFragShader,
													 const char *fogVtxShader,
													 const char *fogFragShader,
													 const char *framebufferOutputVtxShader,
													 const char *framebufferOutputRGBA6665FragShader,
													 const char *framebufferOutputRGBA8888FragShader) = 0;
	virtual Render3DError DestroyPostprocessingPrograms() = 0;
	virtual Render3DError InitEdgeMarkProgramBindings() = 0;
	virtual Render3DError InitEdgeMarkProgramShaderLocations() = 0;
	virtual Render3DError InitFogProgramBindings() = 0;
	virtual Render3DError InitFogProgramShaderLocations() = 0;
	virtual Render3DError InitFramebufferOutputProgramBindings() = 0;
	virtual Render3DError InitFramebufferOutputShaderLocations() = 0;
	
	virtual Render3DError InitGeometryProgramBindings() = 0;
	virtual Render3DError InitGeometryProgramShaderLocations() = 0;
	virtual Render3DError InitGeometryZeroDstAlphaProgramBindings() = 0;
	virtual Render3DError InitGeometryZeroDstAlphaProgramShaderLocations() = 0;
	virtual Render3DError CreateToonTable() = 0;
	virtual Render3DError DestroyToonTable() = 0;
	virtual Render3DError UploadClearImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 *__restrict polyIDBuffer) = 0;
	
	virtual void GetExtensionSet(std::set<std::string> *oglExtensionSet) = 0;
	virtual Render3DError EnableVertexAttributes() = 0;
	virtual Render3DError DisableVertexAttributes() = 0;
	virtual Render3DError DownsampleFBO() = 0;
	virtual Render3DError ReadBackPixels() = 0;
	
	virtual Render3DError DrawShadowPolygon(const GLenum polyPrimitive, const GLsizei vertIndexCount, const GLushort *indexBufferPtr, const bool performDepthEqualTest, const bool enableAlphaDepthWrite, const bool isTranslucent, const u8 opaquePolyID) = 0;
	virtual void SetPolygonIndex(const size_t index) = 0;
	virtual Render3DError SetupPolygon(const POLY &thePoly, bool treatAsTranslucent, bool willChangeStencilBuffer) = 0;
	
public:
	OpenGLRenderer();
	virtual ~OpenGLRenderer();
	
	virtual Render3DError InitExtensions() = 0;
	
	bool IsExtensionPresent(const std::set<std::string> *oglExtensionSet, const std::string extensionName) const;
	bool ValidateShaderCompile(GLuint theShader) const;
	bool ValidateShaderProgramLink(GLuint theProgram) const;
	void GetVersion(unsigned int *major, unsigned int *minor, unsigned int *revision) const;
	void SetVersion(unsigned int major, unsigned int minor, unsigned int revision);
	
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
	virtual Render3DError InitFinalRenderStates(const std::set<std::string> *oglExtensionSet);
	virtual Render3DError InitTables();
	
	virtual Render3DError InitGeometryProgram(const char *geometryVtxShaderCString, const char *geometryFragShaderCString,
											  const char *geometryAlphaVtxShaderCString, const char *geometryAlphaFragShaderCString,
											  const char *geometryMSAlphaVtxShaderCString, const char *geometryMSAlphaFragShaderCString);
	virtual Render3DError InitGeometryProgramBindings();
	virtual Render3DError InitGeometryProgramShaderLocations();
	virtual Render3DError InitGeometryZeroDstAlphaProgramBindings();
	virtual Render3DError InitGeometryZeroDstAlphaProgramShaderLocations();
	virtual void DestroyGeometryProgram();
	virtual Render3DError InitPostprocessingPrograms(const char *edgeMarkVtxShader,
													 const char *edgeMarkFragShader,
													 const char *fogVtxShader,
													 const char *fogFragShader,
													 const char *framebufferOutputVtxShader,
													 const char *framebufferOutputRGBA6665FragShader,
													 const char *framebufferOutputRGBA8888FragShader);
	virtual Render3DError DestroyPostprocessingPrograms();
	virtual Render3DError InitEdgeMarkProgramBindings();
	virtual Render3DError InitEdgeMarkProgramShaderLocations();
	virtual Render3DError InitFogProgramBindings();
	virtual Render3DError InitFogProgramShaderLocations();
	virtual Render3DError InitFramebufferOutputProgramBindings();
	virtual Render3DError InitFramebufferOutputShaderLocations();
	
	virtual Render3DError CreateToonTable();
	virtual Render3DError DestroyToonTable();
	virtual Render3DError UploadClearImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 *__restrict polyIDBuffer);
	
	virtual void GetExtensionSet(std::set<std::string> *oglExtensionSet);
	virtual Render3DError EnableVertexAttributes();
	virtual Render3DError DisableVertexAttributes();
	virtual Render3DError ZeroDstAlphaPass(const POLYLIST *polyList, const INDEXLIST *indexList, bool enableAlphaBlending, size_t indexOffset, POLYGON_ATTR lastPolyAttr);
	virtual Render3DError DownsampleFBO();
	virtual Render3DError ReadBackPixels();
	
	// Base rendering methods
	virtual Render3DError BeginRender(const GFX3D &engine);
	virtual Render3DError RenderGeometry(const GFX3D_State &renderState, const POLYLIST *polyList, const INDEXLIST *indexList);
	virtual Render3DError RenderEdgeMarking(const u16 *colorTable, const bool useAntialias);
	virtual Render3DError RenderFog(const u8 *densityTable, const u32 color, const u32 offset, const u8 shift, const bool alphaOnly);
	virtual Render3DError EndRender(const u64 frameCount);
	
	virtual Render3DError ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 *__restrict polyIDBuffer);
	virtual Render3DError ClearUsingValues(const FragmentColor &clearColor6665, const FragmentAttributes &clearAttributes);
	
	virtual void SetPolygonIndex(const size_t index);
	virtual Render3DError SetupPolygon(const POLY &thePoly, bool treatAsTranslucent, bool willChangeStencilBuffer);
	virtual Render3DError SetupTexture(const POLY &thePoly, size_t polyRenderIndex);
	virtual Render3DError SetupViewport(const u32 viewportValue);
	
	virtual Render3DError DrawShadowPolygon(const GLenum polyPrimitive, const GLsizei vertIndexCount, const GLushort *indexBufferPtr, const bool performDepthEqualTest, const bool enableAlphaDepthWrite, const bool isTranslucent, const u8 opaquePolyID);
	
public:
	~OpenGLRenderer_1_2();
	
	virtual Render3DError InitExtensions();
	virtual Render3DError UpdateToonTable(const u16 *toonTableBuffer);
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
