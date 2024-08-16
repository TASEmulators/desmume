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

#ifndef OGLRENDER_H
#define OGLRENDER_H

#include <map>
#include <queue>
#include <set>
#include <string>
#include "render3D.h"
#include "types.h"

// The macros ENABLE_OPENGL_STANDARD and ENABLE_OPENGL_ES are used in client build configs.
// If a client wants to use OpenGL, they must declare one of these two macros somewhere in
// their build config or declare it in some precompiled header.

// OPENGL PLATFORM-SPECIFIC INCLUDES
#if defined(_WIN32)
	#ifndef ENABLE_OPENGL_STANDARD
		#define ENABLE_OPENGL_STANDARD // TODO: Declare this in the Visual Studio project file.
	#endif
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <GL/gl.h>
	#include <GL/glext.h>

	#ifdef OGLRENDER_3_2_H
		#include <GL/glcorearb.h>
	#endif

	#define OGLEXT(procPtr, func)       procPtr func = NULL;
	#define INITOGLEXT(procPtr, func)   func = (procPtr)wglGetProcAddress(#func);
	#define EXTERNOGLEXT(procPtr, func) extern procPtr func;
#elif defined(__APPLE__)
	#if defined(ENABLE_OPENGL_ES)
		#include <OpenGLES/ES3/gl.h>
		#include <OpenGLES/ES3/glext.h>
	#elif defined(ENABLE_OPENGL_STANDARD)
		#ifdef OGLRENDER_3_2_H
			#include <OpenGL/gl3.h>
			#include <OpenGL/gl3ext.h>
		#else
			#include <OpenGL/gl.h>
			#include <OpenGL/glext.h>
		#endif
		
		// Use Apple's extension function if the core function is not available
		#if defined(GL_APPLE_vertex_array_object) && !defined(GL_ARB_vertex_array_object)
			#define glGenVertexArrays(n, ids)    glGenVertexArraysAPPLE(n, ids)
			#define glBindVertexArray(id)        glBindVertexArrayAPPLE(id)
			#define glDeleteVertexArrays(n, ids) glDeleteVertexArraysAPPLE(n, ids)
		#endif
	#endif
	
	// Ignore dynamic linking
	#define OGLEXT(procPtr, func)
	#define INITOGLEXT(procPtr, func)
	#define EXTERNOGLEXT(procPtr, func)
#else
	#if defined(ENABLE_OPENGL_ES)
		#include <GLES3/gl3.h>
		#define __gles2_gl2_h_ // Guard against including the gl2.h file.
		#include <GLES2/gl2ext.h> // "gl3ext.h" is just a stub file. The real extension header is "gl2ext.h".

		// Ignore dynamic linking
		#define OGLEXT(procPtr, func)
		#define INITOGLEXT(procPtr, func)
		#define EXTERNOGLEXT(procPtr, func)
	#elif defined(ENABLE_OPENGL_STANDARD)
		#include <GL/gl.h>
		#include <GL/glext.h>
		#include <GL/glx.h>

		#ifdef OGLRENDER_3_2_H
			#include "utils/glcorearb.h"
		#else
			// This is a workaround needed to compile against nvidia GL headers
			#ifndef GL_ALPHA_BLEND_EQUATION_ATI
				#undef GL_VERSION_1_3
			#endif
		#endif

		#define OGLEXT(procPtr, func)       procPtr func = NULL;
		#define INITOGLEXT(procPtr, func)   func = (procPtr)glXGetProcAddress((const GLubyte *) #func);
		#define EXTERNOGLEXT(procPtr, func) extern procPtr func;
	#endif
#endif

// Check minimum OpenGL header version
#if defined(ENABLE_OPENGL_ES)
	#if !defined(GL_ES_VERSION_3_0)
		#error OpenGL ES requires v3.0 headers or later.
	#endif
#elif defined(ENABLE_OPENGL_STANDARD)
	#if !defined(GL_VERSION_2_1)
		#error Standard OpenGL requires v2.1 headers or later.
	#endif
#else
	#error No OpenGL variant selected. You must declare ENABLE_OPENGL_STANDARD or ENABLE_OPENGL_ES in your build configuration.
#endif

// OPENGL LEGACY CORE FUNCTIONS

// Textures
#if !defined(GLX_H)
EXTERNOGLEXT(PFNGLACTIVETEXTUREPROC, glActiveTexture) // Core in v1.3
#endif

// Blending
#if !defined(GLX_H)
EXTERNOGLEXT(PFNGLBLENDCOLORPROC, glBlendColor) // Core in v1.2
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
EXTERNOGLEXT(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) // Core in v2.0
EXTERNOGLEXT(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) // Core in v2.0
EXTERNOGLEXT(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) // Core in v2.0

// Buffer Objects
EXTERNOGLEXT(PFNGLGENBUFFERSPROC, glGenBuffers) // Core in v1.5
EXTERNOGLEXT(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) // Core in v1.5
EXTERNOGLEXT(PFNGLBINDBUFFERPROC, glBindBuffer) // Core in v1.5
EXTERNOGLEXT(PFNGLBUFFERDATAPROC, glBufferData) // Core in v1.5
EXTERNOGLEXT(PFNGLBUFFERSUBDATAPROC, glBufferSubData) // Core in v1.5
#if defined(GL_VERSION_1_5)
EXTERNOGLEXT(PFNGLMAPBUFFERPROC, glMapBuffer) // Core in v1.5
#endif
EXTERNOGLEXT(PFNGLUNMAPBUFFERPROC, glUnmapBuffer) // Core in v1.5

// VAO (always available in Apple's implementation of OpenGL, including old versions)
#if defined(__APPLE__) || defined(GL_VERSION_3_0) || defined(GL_ES_VERSION_3_0)
EXTERNOGLEXT(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) // Core in v3.0 and ES v3.0
EXTERNOGLEXT(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays) // Core in v3.0 and ES v3.0
EXTERNOGLEXT(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) // Core in v3.0 and ES v3.0
#endif

// OPENGL CORE FUNCTIONS ADDED IN 3.2 CORE PROFILE AND VARIANTS
#if defined(GL_VERSION_3_0) || defined(GL_ES_VERSION_3_0)

// Basic Functions
EXTERNOGLEXT(PFNGLGETSTRINGIPROC, glGetStringi) // Core in v3.0 and ES v3.0
EXTERNOGLEXT(PFNGLCLEARBUFFERFVPROC, glClearBufferfv) // Core in v3.0 and ES v3.0
EXTERNOGLEXT(PFNGLCLEARBUFFERFIPROC, glClearBufferfi) // Core in v3.0 and ES v3.0

// Shaders
#if defined(GL_VERSION_3_0)
EXTERNOGLEXT(PFNGLBINDFRAGDATALOCATIONPROC, glBindFragDataLocation) // Core in v3.0, not available in ES
#endif
#if defined(GL_VERSION_3_3) || defined(GL_ARB_blend_func_extended)
EXTERNOGLEXT(PFNGLBINDFRAGDATALOCATIONINDEXEDPROC, glBindFragDataLocationIndexed) // Core in v3.3, not available in ES
#endif

// Buffer Objects
EXTERNOGLEXT(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange) // Core in v3.0 and ES v3.0

// FBO
EXTERNOGLEXT(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) // Core in v3.0 and ES v2.0
EXTERNOGLEXT(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) // Core in v3.0 and ES v2.0
EXTERNOGLEXT(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) // Core in v3.0 and ES v2.0
EXTERNOGLEXT(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) // Core in v3.0 and ES v2.0
EXTERNOGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) // Core in v3.0 and ES v2.0
EXTERNOGLEXT(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) // Core in v3.0 and ES v2.0
EXTERNOGLEXT(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer) // Core in v3.0 and ES v3.0

// Multisampled FBO
EXTERNOGLEXT(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) // Core in v3.0 and ES v2.0
EXTERNOGLEXT(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) // Core in v3.0 and ES v2.0
EXTERNOGLEXT(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) // Core in v3.0 and ES v2.0
EXTERNOGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, glRenderbufferStorageMultisample) // Core in v3.0 and ES v3.0
EXTERNOGLEXT(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers) // Core in v3.0 and ES v2.0
#if defined(GL_VERSION_3_2) || defined(GL_ARB_texture_multisample)
EXTERNOGLEXT(PFNGLTEXIMAGE2DMULTISAMPLEPROC, glTexImage2DMultisample) // Core in v3.2, not available in ES
#endif

// UBO
EXTERNOGLEXT(PFNGLGETUNIFORMBLOCKINDEXPROC, glGetUniformBlockIndex) // Core in v3.1 and ES v3.0
EXTERNOGLEXT(PFNGLUNIFORMBLOCKBINDINGPROC, glUniformBlockBinding) // Core in v3.1 and ES v3.0
EXTERNOGLEXT(PFNGLBINDBUFFERBASEPROC, glBindBufferBase) // Core in v3.0 and ES v3.0
EXTERNOGLEXT(PFNGLGETACTIVEUNIFORMBLOCKIVPROC, glGetActiveUniformBlockiv) // Core in v3.1 and ES v3.0

// TBO
#if defined(GL_VERSION_3_1) || defined(GL_ES_VERSION_3_2)
EXTERNOGLEXT(PFNGLTEXBUFFERPROC, glTexBuffer) // Core in v3.1 and ES v3.2
#endif

// Sync Objects
EXTERNOGLEXT(PFNGLFENCESYNCPROC, glFenceSync) // Core in v3.2 and ES v3.0
EXTERNOGLEXT(PFNGLWAITSYNCPROC, glWaitSync) // Core in v3.2 and ES v3.0
EXTERNOGLEXT(PFNGLCLIENTWAITSYNCPROC, glClientWaitSync) // Core in v3.2 and ES v3.0
EXTERNOGLEXT(PFNGLDELETESYNCPROC, glDeleteSync) // Core in v3.2 and ES v3.0

#endif // GL_VERSION_3_0 || GL_ES_VERSION_3_0

// OPENGL FBO EXTENSIONS
// We need to include these explicitly for OpenGL legacy mode since the EXT versions of FBOs
// may work differently than their ARB counterparts when running on older drivers.
#if defined(GL_EXT_framebuffer_object)
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

#elif defined(GL_ARB_framebuffer_object) || defined(GL_ES_VERSION_3_0)
// Most OpenGL variants don't have GL_EXT_framebuffer_object, so redeclare all the ARB versions
// to their EXT versions to avoid compile time errors in OGLRender.cpp.
//
// In practice, class objects for more modern variants like 3.2 Core Profile and ES 3.0 should
// override all the methods that would use FBOs so that only the ARB versions are actually used.

#ifndef GL_EXT_draw_buffers
#define GL_MAX_COLOR_ATTACHMENTS_EXT GL_MAX_COLOR_ATTACHMENTS
#define GL_COLOR_ATTACHMENT0_EXT     GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT1_EXT     GL_COLOR_ATTACHMENT1
#define GL_COLOR_ATTACHMENT2_EXT     GL_COLOR_ATTACHMENT2
#define GL_COLOR_ATTACHMENT3_EXT     GL_COLOR_ATTACHMENT3
#endif

#ifndef GL_EXT_multisampled_render_to_texture
#define GL_MAX_SAMPLES_EXT           GL_MAX_SAMPLES
#endif

#define GL_DEPTH24_STENCIL8_EXT      GL_DEPTH24_STENCIL8
#define GL_DEPTH_STENCIL_EXT         GL_DEPTH_STENCIL
#define GL_UNSIGNED_INT_24_8_EXT     GL_UNSIGNED_INT_24_8
#define GL_FRAMEBUFFER_EXT           GL_FRAMEBUFFER
#define GL_FRAMEBUFFER_COMPLETE_EXT  GL_FRAMEBUFFER_COMPLETE
#define GL_DEPTH_ATTACHMENT_EXT      GL_DEPTH_ATTACHMENT
#define GL_STENCIL_ATTACHMENT_EXT    GL_STENCIL_ATTACHMENT
#define GL_RENDERBUFFER_EXT          GL_RENDERBUFFER
#define GL_DRAW_FRAMEBUFFER_EXT      GL_DRAW_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER_EXT      GL_READ_FRAMEBUFFER

#define glGenFramebuffersEXT(n, framebuffers) glGenFramebuffers(n, framebuffers)
#define glBindFramebufferEXT(target, framebuffer) glBindFramebuffer(target, framebuffer)
#define glFramebufferRenderbufferEXT(target, attachment, renderbuffertarget, renderbuffer) glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer)
#define glFramebufferTexture2DEXT(target, attachment, textarget, texture, level) glFramebufferTexture2D(target, attachment, textarget, texture, level)
#define glCheckFramebufferStatusEXT(target) glCheckFramebufferStatus(target)
#define glDeleteFramebuffersEXT(n, framebuffers) glDeleteFramebuffers(n, framebuffers)
#define glBlitFramebufferEXT(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter) glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter)

#define glGenRenderbuffersEXT(n, renderbuffers) glGenRenderbuffers(n, renderbuffers)
#define glBindRenderbufferEXT(target, renderbuffer) glBindRenderbuffer(target, renderbuffer)
#define glRenderbufferStorageEXT(target, internalformat, width, height) glRenderbufferStorage(target, internalformat, width, height)
#define glRenderbufferStorageMultisampleEXT(target, samples, internalformat, width, height) glRenderbufferStorageMultisample(target, samples, internalformat, width, height)
#define glDeleteRenderbuffersEXT(n, renderbuffers) glDeleteRenderbuffers(n, renderbuffers)

#endif // GL_EXT_framebuffer_object

// Some headers, such as the OpenGL ES headers, may not include this token.
// Add it manually to avoid compiling issues.
#ifndef GL_BGRA
	#define GL_BGRA GL_BGRA_EXT
#endif

// OpenGL ES headers before 3.2 don't have this token, so manually define it here.
#ifndef GL_TEXTURE_BUFFER
	#define GL_TEXTURE_BUFFER 0x8C2A
#endif

// OPENGL CORE EQUIVALENTS FOR LEGACY FUNCTIONS
// Some OpenGL variants, such as OpenGL ES, do not include certain legacy functions in their
// API. The loss of these functions will cause compile time errors when referenced, and so
// we will redeclare them to something that is supported for the sake of compiling.
//
// Do note that a redeclared function will most likely not work in exactly the same way as
// a native legacy version, and so implmentations are responsible for overriding any methods
// that use these legacy functions to whatever is available in the specific OpenGL variant.

#ifndef GL_VERSION_1_2
	// These legacy functions can be promoted to later core equivalents without any further
	// modification. In other words, these are one-to-one drop-in replacements.
	typedef GLclampf GLclampd;
	#define glClearDepth(depth) glClearDepthf(depth)

	// 1D textures may not exist for a particular OpenGL variant, so they will be promoted to
	// 2D textures instead. Implementations need to modify their GLSL shaders accordingly to
	// treat any 1D textures as 2D textures instead.
	#define GL_TEXTURE_1D GL_TEXTURE_2D
	#define glTexImage1D(target, level, internalformat, width, border, format, type, pixels) glTexImage2D(target, level, internalformat, width, 1, border, format, type, pixels)
	#define glTexSubImage1D(target, level, xoffset, width, format, type, pixels) glTexSubImage2D(target, level, xoffset, 0, width, 1, format, type, pixels)
#endif

#ifndef GL_VERSION_1_5
	// Calling glMapBuffer with an OpenGL variant that forgoes legacy functions will cause a
	// GL_INVALID_OPERATION error if used in practice. Implementations need to override any
	// methods that would call glMapBuffer with glMapBufferRange.
	#define GL_READ_ONLY  GL_MAP_READ_BIT
	#define GL_WRITE_ONLY GL_MAP_WRITE_BIT
	#define glMapBuffer(target, access) glMapBufferRange(target, 0, 0, access)
#endif

// Define the minimum required OpenGL version for the context to support
#define OGLRENDER_STANDARD_MINIMUM_CONTEXT_VERSION_REQUIRED_MAJOR    1
#define OGLRENDER_STANDARD_MINIMUM_CONTEXT_VERSION_REQUIRED_MINOR    2
#define OGLRENDER_STANDARD_MINIMUM_CONTEXT_VERSION_REQUIRED_REVISION 0
#define OGLRENDER_ES_MINIMUM_CONTEXT_VERSION_REQUIRED_MAJOR    3
#define OGLRENDER_ES_MINIMUM_CONTEXT_VERSION_REQUIRED_MINOR    0
#define OGLRENDER_ES_MINIMUM_CONTEXT_VERSION_REQUIRED_REVISION 0

#define OGLRENDER_VERT_INDEX_BUFFER_COUNT	(CLIPPED_POLYLIST_SIZE * 6)

// Assign the FBO attachments for the main geometry render
#if defined(GL_VERSION_3_0) || defined(GL_ES_VERSION_3_0)
	#define OGL_COLOROUT_ATTACHMENT_ID         GL_COLOR_ATTACHMENT0
	#define OGL_WORKING_ATTACHMENT_ID          GL_COLOR_ATTACHMENT1
	#define OGL_POLYID_ATTACHMENT_ID           GL_COLOR_ATTACHMENT2
	#define OGL_FOGATTRIBUTES_ATTACHMENT_ID    GL_COLOR_ATTACHMENT3

	#define OGL_CI_COLOROUT_ATTACHMENT_ID      GL_COLOR_ATTACHMENT0
	#define OGL_CI_FOGATTRIBUTES_ATTACHMENT_ID GL_COLOR_ATTACHMENT1
#else
	#define OGL_COLOROUT_ATTACHMENT_ID         GL_COLOR_ATTACHMENT0_EXT
	#define OGL_WORKING_ATTACHMENT_ID          GL_COLOR_ATTACHMENT1_EXT
	#define OGL_POLYID_ATTACHMENT_ID           GL_COLOR_ATTACHMENT2_EXT
	#define OGL_FOGATTRIBUTES_ATTACHMENT_ID    GL_COLOR_ATTACHMENT3_EXT

	#define OGL_CI_COLOROUT_ATTACHMENT_ID      GL_COLOR_ATTACHMENT0_EXT
	#define OGL_CI_FOGATTRIBUTES_ATTACHMENT_ID GL_COLOR_ATTACHMENT1_EXT
#endif

enum OpenGLVariantID
{
	OpenGLVariantID_Unknown         = 0,
	OpenGLVariantID_LegacyAuto      = 0x1000,
	OpenGLVariantID_Legacy_1_2      = 0x1012,
	OpenGLVariantID_Legacy_1_3      = 0x1013,
	OpenGLVariantID_Legacy_1_4      = 0x1014,
	OpenGLVariantID_Legacy_1_5      = 0x1015,
	OpenGLVariantID_Legacy_2_0      = 0x1020,
	OpenGLVariantID_Legacy_2_1      = 0x1021,
	OpenGLVariantID_CoreProfile_3_2 = 0x2032,
	OpenGLVariantID_CoreProfile_3_3 = 0x2033,
	OpenGLVariantID_CoreProfile_4_0 = 0x2040,
	OpenGLVariantID_CoreProfile_4_1 = 0x2041,
	OpenGLVariantID_CoreProfile_4_2 = 0x2042,
	OpenGLVariantID_CoreProfile_4_3 = 0x2043,
	OpenGLVariantID_CoreProfile_4_4 = 0x2044,
	OpenGLVariantID_CoreProfile_4_5 = 0x2045,
	OpenGLVariantID_CoreProfile_4_6 = 0x2046,
	OpenGLVariantID_StandardAuto    = 0x3000,
	OpenGLVariantID_ES3_Auto        = 0x4000,
	OpenGLVariantID_ES3_3_0         = 0x4030,
	OpenGLVariantID_ES3_3_1         = 0x4031,
	OpenGLVariantID_ES3_3_2         = 0x4032,
	OpenGLVariantID_ES_Auto         = 0x6000
};

enum OpenGLVariantFamily
{
	OpenGLVariantFamily_Standard    = (3 << 12),
	OpenGLVariantFamily_Legacy      = (1 << 12),
	OpenGLVariantFamily_CoreProfile = (1 << 13),
	OpenGLVariantFamily_ES          = (3 << 14),
	OpenGLVariantFamily_ES3         = (1 << 14)
};

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
	OGLTextureUnitID_LookupTable,
	OGLTextureUnitID_CIColor,
	OGLTextureUnitID_CIDepth,
	OGLTextureUnitID_CIFogAttr
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
	GLfloat vec[2];
	struct { GLfloat x, y; };
};
typedef union GLvec2 GLvec2;

union GLvec3
{
	GLfloat vec[3];
	struct { GLfloat r, g, b; };
	struct { GLfloat x, y, z; };
};
typedef union GLvec3 GLvec3;

union GLvec4
{
	GLfloat vec[4];
	struct { GLfloat r, g, b, a; };
	struct { GLfloat x, y, z, w; };
};
typedef union GLvec4 GLvec4;

struct OGLVertex
{
	GLvec4 position;
	GLvec2 texCoord;
	GLvec3 color;
};
typedef struct OGLVertex OGLVertex;

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
typedef struct OGLRenderStates OGLRenderStates;

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
		
		u8 IsBackFacing:1;
		u8 :7;
	};
};
typedef union OGLPolyStates OGLPolyStates;

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
	GLint readPixelsBestDataType;
	GLint readPixelsBestFormat;
	GLenum textureSrcTypeCIColor;
	GLenum textureSrcTypeCIFog;
	GLenum textureSrcTypeEdgeColor;
	GLenum textureSrcTypeToonTable;
	
	// VBO
	GLuint vboGeometryVtxID;
	GLuint iboGeometryIndexID;
	GLuint vboPostprocessVtxID;
	
	// PBO
	GLuint pboRenderDataID;
	
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
	GLuint texToonTableID;
	GLuint texEdgeColorTableID;
	GLuint texMSGColorID;
	GLuint texMSGWorkingID;
	GLuint texMSGDepthStencilID;
	GLuint texMSGPolyID;
	GLuint texMSGFogAttrID;
	
	GLuint rboMSGColorID;
	GLuint rboMSGWorkingID;
	GLuint rboMSGPolyID;
	GLuint rboMSGFogAttrID;
	GLuint rboMSGDepthStencilID;
	
	GLuint fboClearImageID;
	GLuint fboRenderID;
	GLuint fboMSClearImageID;
	GLuint fboMSIntermediateRenderID;
	GLuint selectedRenderingFBO;
	
	// Shader states
	GLuint vertexGeometryShaderID;
	GLuint fragmentGeometryShaderID[128];
	GLuint programGeometryID[128];
	
	GLuint vtxShaderGeometryZeroDstAlphaID;
	GLuint fragShaderGeometryZeroDstAlphaID;
	GLuint programGeometryZeroDstAlphaID;
	
	GLuint vsClearImageID;
	GLuint fsClearImageID;
	GLuint pgClearImageID;
	
	GLuint vtxShaderMSGeometryZeroDstAlphaID;
	GLuint fragShaderMSGeometryZeroDstAlphaID;
	GLuint programMSGeometryZeroDstAlphaID;
	
	GLuint vertexMSEdgeMarkShaderID;
	GLuint fragmentMSEdgeMarkShaderID;
	GLuint programMSEdgeMarkID;
	
	GLuint vertexEdgeMarkShaderID;
	GLuint vertexFogShaderID;
	GLuint vertexFramebufferOutput6665ShaderID;
	GLuint vertexFramebufferOutput8888ShaderID;
	GLuint fragmentEdgeMarkShaderID;
	GLuint fragmentFramebufferRGBA6665OutputShaderID;
	GLuint fragmentFramebufferRGBA8888OutputShaderID;
	GLuint programEdgeMarkID;
	GLuint programFramebufferRGBA6665OutputID;
	GLuint programFramebufferRGBA8888OutputID;
	
	GLint uniformStateEnableFogAlphaOnly;
	GLint uniformStateClearPolyID;
	GLint uniformStateClearDepth;
	
	GLint uniformStateAlphaTestRef[256];
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
	GLint uniformPolyIsBackFacing[256];
	
	GLint uniformPolyStateIndex[256];
	GLfloat uniformPolyDepthOffset[256];
	GLint uniformPolyDrawShadow[256];
	
	// VAO
	GLuint vaoGeometryStatesID;
	GLuint vaoPostprocessStatesID;
	
	// Client-side Buffers
	GLfloat *position4fBuffer;
	GLfloat *texCoord2fBuffer;
	GLfloat *color4fBuffer;
	CACHE_ALIGN GLushort vertIndexBuffer[OGLRENDER_VERT_INDEX_BUFFER_COUNT];
	CACHE_ALIGN GLuint toonTable32[32];
	CACHE_ALIGN GLushort workingCIColorBuffer16[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	CACHE_ALIGN GLuint workingCIColorBuffer32[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	CACHE_ALIGN GLuint workingCIDepthStencilBuffer[2][GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	CACHE_ALIGN GLuint workingCIFogAttributesBuffer[2][GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
};
typedef struct OGLRenderRef OGLRenderRef;

struct GFX3D_State;
struct POLY;
class OpenGLRenderer;

extern GPU3DInterface gpu3Dgl;
extern GPU3DInterface gpu3DglOld;
extern GPU3DInterface gpu3Dgl_3_2;
extern GPU3DInterface gpu3Dgl_ES_3_0;

extern CACHE_ALIGN const GLfloat divide5bitBy31_LUT[32];
extern CACHE_ALIGN const GLfloat divide6bitBy63_LUT[64];
extern const GLfloat PostprocessVtxBuffer[16];
extern const GLubyte PostprocessElementBuffer[6];

//This is called by OGLRender whenever it initializes.
//Platforms, please be sure to set this up.
//return true if you successfully init.
extern bool (*oglrender_init)();

// This is called by OGLRender whenever the renderer is switched
// away from this one, or if the renderer is shut down.
extern void (*oglrender_deinit)();

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

// These functions need to be assigned by ports that support using an
// OpenGL ES 3.0 context. The OGLRender_ES3.cpp file includes the corresponding
// functions to assign to each function pointer.
//
// If any of these functions are unassigned, then the renderer object
// creation will fail.
extern void (*OGLLoadEntryPoints_ES_3_0_Func)();
extern void (*OGLCreateRenderer_ES_3_0_Func)(OpenGLRenderer **rendererPtr);

bool IsOpenGLDriverVersionSupported(unsigned int checkVersionMajor, unsigned int checkVersionMinor, unsigned int checkVersionRevision);

#define glDrawBuffer(theAttachment) glDrawBufferDESMUME((theAttachment), this->_variantID)
static inline void glDrawBufferDESMUME(GLenum theAttachment, const OpenGLVariantID variantID)
{
	GLenum bufs[4] = { GL_NONE, GL_NONE, GL_NONE, GL_NONE };

	if (variantID & OpenGLVariantFamily_ES)
	{
		switch (theAttachment)
		{
			case GL_NONE:
				glDrawBuffers(1, bufs);
				return;

			case GL_COLOR_ATTACHMENT0_EXT:
			case GL_COLOR_ATTACHMENT1_EXT:
			case GL_COLOR_ATTACHMENT2_EXT:
			case GL_COLOR_ATTACHMENT3_EXT:
			{
				const GLsizei i = theAttachment - GL_COLOR_ATTACHMENT0_EXT;
				bufs[i] = theAttachment;
				glDrawBuffers(i+1, bufs);
				return;
			}

			default:
				return;
		}
	}
	else
	{
		bufs[0] = theAttachment;
		glDrawBuffers(1, bufs);
	}
}

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
#elif defined(ENABLE_NEON_A64)
class OpenGLRenderer : public Render3D_NEON
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
	template<bool SWAP_RB> Render3DError _FlushFramebufferConvertOnCPU(const Color4u8 *__restrict srcFramebuffer,
	                                                                   Color4u8 *__restrict dstFramebufferMain, u16 *__restrict dstFramebuffer16,
	                                                                   bool doFramebufferConvert);
	
protected:
	// OpenGL-specific References
	OGLRenderRef *ref;
	
	// OpenGL Feature Support
	OpenGLVariantID _variantID;
	bool _isBlendFuncSeparateSupported;
	bool _isBlendEquationSeparateSupported;
	bool isVBOSupported;
	bool isPBOSupported;
	bool isFBOSupported;
	bool _isFBOBlitSupported;
	bool isMultisampledFBOSupported;
	bool isShaderSupported;
	bool _isSampleShadingSupported;
	bool isVAOSupported;
	bool _willConvertFramebufferOnGPU;
	bool _willUseMultisampleShaders;
	
	bool _emulateShadowPolygon;
	bool _emulateSpecialZeroAlphaBlending;
	bool _emulateNDSDepthCalculation;
	bool _emulateDepthLEqualPolygonFacing;
	bool _isDepthLEqualPolygonFacingSupported;
	
	const GLenum (*_geometryDrawBuffersEnum)[4];
	const GLint *_geometryAttachmentWorkingBuffer;
	const GLint *_geometryAttachmentPolyID;
	const GLint *_geometryAttachmentFogAttributes;
	
	Color4u8 *_mappedFramebuffer;
	Color4u8 *_workingTextureUnpackBuffer;
	bool _pixelReadNeedsFinish;
	bool _needsZeroDstAlphaPass;
	size_t _currentPolyIndex;
	bool _enableAlphaBlending;
	OGLGeometryFlags _geometryProgramFlags;
	OGLFogProgramKey _fogProgramKey;
	std::map<u32, OGLFogShaderID> _fogProgramMap;
	
	CACHE_ALIGN OGLRenderStates _pendingRenderStates;
	
	bool _enableMultisampledRendering;
	int _selectedMultisampleSize;
	size_t _clearImageIndex;
	
	Render3DError FlushFramebuffer(const Color4u8 *__restrict srcFramebuffer, Color4u8 *__restrict dstFramebufferMain, u16 *__restrict dstFramebuffer16);
	OpenGLTexture* GetLoadedTextureFromPolygon(const POLY &thePoly, bool enableTexturing);
	
	template<OGLPolyDrawMode DRAWMODE> size_t DrawPolygonsForIndexRange(const POLY *rawPolyList, const CPoly *clippedPolyList, const size_t clippedPolyCount, size_t firstIndex, size_t lastIndex, size_t &indexOffset, POLYGON_ATTR &lastPolyAttr);
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
	virtual Render3DError CreateClearImageProgram(const char *vsCString, const char *fsCString) = 0;
	virtual void DestroyClearImageProgram() = 0;
	virtual Render3DError CreateGeometryZeroDstAlphaProgram(const char *vtxShaderCString, const char *fragShaderCString) = 0;
	virtual void DestroyGeometryZeroDstAlphaProgram() = 0;
	virtual Render3DError CreateEdgeMarkProgram(const bool isMultisample, const char *vtxShaderCString, const char *fragShaderCString) = 0;
	virtual void DestroyEdgeMarkProgram() = 0;
	virtual Render3DError CreateFogProgram(const OGLFogProgramKey fogProgramKey, const bool isMultisample, const char *vtxShaderCString, const char *fragShaderCString) = 0;
	virtual void DestroyFogProgram(const OGLFogProgramKey fogProgramKey) = 0;
	virtual void DestroyFogPrograms() = 0;
	virtual Render3DError CreateFramebufferOutput6665Program(const char *vtxShaderCString, const char *fragShaderCString) = 0;
	virtual void DestroyFramebufferOutput6665Programs() = 0;
	virtual Render3DError CreateFramebufferOutput8888Program(const char *vtxShaderCString, const char *fragShaderCString) = 0;
	virtual void DestroyFramebufferOutput8888Programs() = 0;
	
	virtual Render3DError InitPostprocessingPrograms(const char *edgeMarkVtxShader,
													 const char *edgeMarkFragShader,
													 const char *framebufferOutputVtxShader,
													 const char *framebufferOutputRGBA6665FragShader,
													 const char *framebufferOutputRGBA8888FragShader) = 0;
	
	virtual Render3DError UploadClearImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID) = 0;
	
	virtual void GetExtensionSet(std::set<std::string> *oglExtensionSet) = 0;
	virtual void _SetupGeometryShaders(const OGLGeometryFlags flags) = 0;
	virtual void _RenderGeometryVertexAttribEnable() = 0;
	virtual void _RenderGeometryVertexAttribDisable() = 0;
	virtual void _RenderGeometryLoopBegin() = 0;
	virtual void _RenderGeometryLoopEnd() = 0;
	virtual void _ResolveWorkingBackFacing() = 0;
	virtual void _ResolveGeometry() = 0;
	virtual void _ResolveFinalFramebuffer() = 0;
	virtual void _FramebufferProcessVertexAttribEnable() = 0;
	virtual void _FramebufferProcessVertexAttribDisable() = 0;
	virtual Render3DError _FramebufferConvertColorFormat() = 0;
	
	virtual Render3DError DrawShadowPolygon(const GLenum polyPrimitive, const GLsizei vertIndexCount, const GLushort *indexBufferPtr, const bool performDepthEqualTest, const bool enableAlphaDepthWrite, const bool isTranslucent, const u8 opaquePolyID) = 0;
	virtual void SetPolygonIndex(const size_t index) = 0;
	virtual Render3DError SetupPolygon(const POLY &thePoly, bool treatAsTranslucent, bool willChangeStencilBuffer, bool isBackFacing) = 0;
	
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
	
	virtual Color4u8* GetFramebuffer();
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
	virtual Render3DError CreateClearImageProgram(const char *vsCString, const char *fsCString);
	virtual void DestroyClearImageProgram();
	virtual Render3DError CreateGeometryZeroDstAlphaProgram(const char *vtxShaderCString, const char *fragShaderCString);
	virtual void DestroyGeometryZeroDstAlphaProgram();
	virtual Render3DError CreateEdgeMarkProgram(const bool isMultisample, const char *vtxShaderCString, const char *fragShaderCString);
	virtual void DestroyEdgeMarkProgram();
	virtual Render3DError CreateFogProgram(const OGLFogProgramKey fogProgramKey, const bool isMultisample, const char *vtxShaderCString, const char *fragShaderCString);
	virtual void DestroyFogProgram(const OGLFogProgramKey fogProgramKey);
	virtual void DestroyFogPrograms();
	virtual Render3DError CreateFramebufferOutput6665Program(const char *vtxShaderCString, const char *fragShaderCString);
	virtual void DestroyFramebufferOutput6665Programs();
	virtual Render3DError CreateFramebufferOutput8888Program(const char *vtxShaderCString, const char *fragShaderCString);
	virtual void DestroyFramebufferOutput8888Programs();
	
	virtual Render3DError InitPostprocessingPrograms(const char *edgeMarkVtxShader,
													 const char *edgeMarkFragShader,
													 const char *framebufferOutputVtxShader,
													 const char *framebufferOutputRGBA6665FragShader,
													 const char *framebufferOutputRGBA8888FragShader);
	
	virtual Render3DError UploadClearImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID);
	
	virtual void GetExtensionSet(std::set<std::string> *oglExtensionSet);
	virtual void _SetupGeometryShaders(const OGLGeometryFlags flags);
	virtual void _RenderGeometryVertexAttribEnable();
	virtual void _RenderGeometryVertexAttribDisable();
	virtual Render3DError ZeroDstAlphaPass(const POLY *rawPolyList, const CPoly *clippedPolyList, const size_t clippedPolyCount, const size_t clippedPolyOpaqueCount, bool enableAlphaBlending, size_t indexOffset, POLYGON_ATTR lastPolyAttr);
	virtual void _RenderGeometryLoopBegin();
	virtual void _RenderGeometryLoopEnd();
	virtual void _ResolveWorkingBackFacing();
	virtual void _ResolveGeometry();
	virtual void _ResolveFinalFramebuffer();
	virtual void _FramebufferProcessVertexAttribEnable();
	virtual void _FramebufferProcessVertexAttribDisable();
	virtual Render3DError _FramebufferConvertColorFormat();
	
	// Base rendering methods
	virtual Render3DError BeginRender(const GFX3D_State &renderState, const GFX3D_GeometryList &renderGList);
	virtual Render3DError RenderGeometry();
	virtual Render3DError PostprocessFramebuffer();
	virtual Render3DError EndRender();
	
	virtual Render3DError ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID);
	virtual Render3DError ClearUsingValues(const Color4u8 &clearColor6665, const FragmentAttributes &clearAttributes);
	
	virtual void SetPolygonIndex(const size_t index);
	virtual Render3DError SetupPolygon(const POLY &thePoly, bool treatAsTranslucent, bool willChangeStencilBuffer, bool isBackFacing);
	virtual Render3DError SetupTexture(const POLY &thePoly, size_t polyRenderIndex);
	virtual Render3DError SetupViewport(const GFX3D_Viewport viewport);
	
	virtual Render3DError DrawShadowPolygon(const GLenum polyPrimitive, const GLsizei vertIndexCount, const GLushort *indexBufferPtr, const bool performDepthEqualTest, const bool enableAlphaDepthWrite, const bool isTranslucent, const u8 opaquePolyID);
	
public:
	OpenGLRenderer_1_2();
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
public:
	OpenGLRenderer_2_0();
	
protected:
	virtual Render3DError BeginRender(const GFX3D_State &renderState, const GFX3D_GeometryList &renderGList);
};

class OpenGLRenderer_2_1 : public OpenGLRenderer_2_0
{
public:
	OpenGLRenderer_2_1();
};

#endif // OGLRENDER_H
