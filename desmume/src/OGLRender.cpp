/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2013 DeSmuME team

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

//problem - alpha-on-alpha texture rendering might work but the dest alpha buffer isnt tracked correctly
//due to zeromus not having any idea how to set dest alpha blending in opengl.
//so, it doesnt composite to 2d correctly.
//(re: new super mario brothers renders the stormclouds at the beginning)
//!!! fixed on rev.3996

#include <queue>

#include "OGLRender.h"
#include "debug.h"

#define OGLRENDER_MAX_MULTISAMPLES	16
#define VERT_INDEX_BUFFER_SIZE		8192

bool (*oglrender_init)() = NULL;
bool (*oglrender_beginOpenGL)() = NULL;
void (*oglrender_endOpenGL)() = NULL;

static bool BEGINGL() {
	if(oglrender_beginOpenGL) 
		return oglrender_beginOpenGL();
	else return true;
}

static void ENDGL() {
	if(oglrender_endOpenGL) 
		oglrender_endOpenGL();
}

#if defined(_WIN32) && !defined(WXPORT)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <GL/gl.h>
	#include <GL/glext.h>
#elif defined(__APPLE__)
	#include <OpenGL/gl.h>
	#include <OpenGL/glext.h>
	
	// Overrides for GL_EXT_framebuffer_blit (not available in Mac OS X v10.4)
	#if !defined(GL_ARB_framebuffer_object) && !defined(GL_EXT_framebuffer_blit)
		#define GL_READ_FRAMEBUFFER_EXT GL_FRAMEBUFFER_EXT
		#define GL_DRAW_FRAMEBUFFER_EXT GL_FRAMEBUFFER_EXT
		#define glBlitFramebufferEXT(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter)
	#endif
	
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
	/* This is a workaround needed to compile against nvidia GL headers */
	#ifndef GL_ALPHA_BLEND_EQUATION_ATI
		#undef GL_VERSION_1_3
	#endif
#endif

// Check minimum OpenGL header version
#if !defined(GL_VERSION_2_0)
	#error OpenGL requires v2.0 headers or later.
#endif

// Define minimum OpenGL version for GPU
#define OGL_MINIMUM_GPU_VERSION_REQUIRED_MAJOR		1
#define OGL_MINIMUM_GPU_VERSION_REQUIRED_MINOR		2
#define OGL_MINIMUM_GPU_VERSION_REQUIRED_REVISION	0

#include "types.h"
#include "debug.h"
#include "MMU.h"
#include "bits.h"
#include "matrix.h"
#include "NDSSystem.h"
#include "OGLRender.h"
#include "gfx3d.h"

#include "shaders.h"
#include "texcache.h"
#include "utils/task.h"


enum OGLVertexAttributeID
{
	OGLVertexAttributeID_Position	= 0,
	OGLVertexAttributeID_TexCoord0	= 8,
	OGLVertexAttributeID_Color		= 3,
};

enum OGLTextureUnitID
{
	// Main textures will always be on texture unit 0.
	OGLTextureUnitID_ToonTable = 1,
	OGLTextureUnitID_ClearImage
};

// Multithreading States
static bool enableMultithreading = false;
static Task oglReadPixelsTask[2];

// Polygon Info
static GLfloat polyAlpha = 1.0f;

// OpenGL Feature Support
static bool isVBOSupported = false;
static bool isPBOSupported = false;
static bool isFBOSupported = false;
static bool isMultisampledFBOSupported = false;
static bool isShaderSupported = false;
static bool isVAOSupported = false;

// VBO
static GLuint vboVertexID;

// PBO
static GLuint pboRenderDataID[2];
static u32 *__restrict pboRenderBuffer[2] = {NULL, NULL};

// FBO
static GLuint texClearImageColorID;
static GLuint texClearImageDepthStencilID;
static GLuint fboClearImageID;

// Multisampled FBO
static GLuint rboMultisampleColorID;
static GLuint rboMultisampleDepthStencilID;
static GLuint fboMultisampleRenderID;
static GLuint selectedRenderingFBO = 0;

// Shader states
static GLuint vertexShaderID;
static GLuint fragmentShaderID;
static GLuint shaderProgram;

static GLint uniformPolyID;
static GLint uniformPolyAlpha;
static GLint uniformTexScale;
static GLint uniformHasTexture;
static GLint uniformPolygonMode;
static GLint uniformToonShadingMode;
static GLint uniformWBuffer;
static GLint uniformEnableAlphaTest;
static GLint uniformAlphaTestRef;

static GLuint texToonTableID;
static u16 currentToonTable16[32];
static bool toonTableNeedsUpdate = true;

// VAO
static GLuint vaoMainStatesID;

// Textures
static bool hasTexture = false;
static TexCacheItem* currTexture = NULL;
static std::queue<GLuint> freeTextureIds;

// Client-side Buffers
static GLfloat *color4fBuffer = NULL;
static GLushort *vertIndexBuffer = NULL;

static DS_ALIGN(16) u32 GPU_screen3D[2][256 * 192 * sizeof(u32)];
static bool gpuScreen3DHasNewData[2] = {false, false};
static unsigned int gpuScreen3DBufferIndex = 0;

// Lookup Tables
static CACHE_ALIGN GLfloat material_8bit_to_float[255] = {0};
static CACHE_ALIGN GLuint dsDepthToD24S8_LUT[32768] = {0};
static const GLfloat divide5bitBy31LUT[32] =	{0.0, 0.03225806451613, 0.06451612903226, 0.09677419354839,
												 0.1290322580645, 0.1612903225806, 0.1935483870968, 0.2258064516129,
												 0.258064516129, 0.2903225806452, 0.3225806451613, 0.3548387096774,
												 0.3870967741935, 0.4193548387097, 0.4516129032258, 0.4838709677419,
												 0.5161290322581, 0.5483870967742, 0.5806451612903, 0.6129032258065,
												 0.6451612903226, 0.6774193548387, 0.7096774193548, 0.741935483871,
												 0.7741935483871, 0.8064516129032, 0.8387096774194, 0.8709677419355,
												 0.9032258064516, 0.9354838709677, 0.9677419354839, 1.0};

//------------------------------------------------------------

#define OGLEXT(x,y) x y = 0;

#ifdef _WIN32
#define INITOGLEXT(x,y) y = (x)wglGetProcAddress(#y);
#elif !defined(__APPLE__)
#include <GL/glx.h>
#define INITOGLEXT(x,y) y = (x)glXGetProcAddress((const GLubyte *) #y);
#endif

#ifndef __APPLE__
OGLEXT(PFNGLCREATESHADERPROC,glCreateShader)
//zero: i dont understand this at all. my glext.h has the wrong thing declared here... so I have to do it myself
typedef void (APIENTRYP X_PFNGLGETSHADERSOURCEPROC) (GLuint shader, GLsizei bufSize, const GLchar **source, GLsizei *length);
OGLEXT(X_PFNGLGETSHADERSOURCEPROC,glShaderSource)
OGLEXT(PFNGLCOMPILESHADERPROC,glCompileShader)
OGLEXT(PFNGLCREATEPROGRAMPROC,glCreateProgram)
OGLEXT(PFNGLATTACHSHADERPROC,glAttachShader)
OGLEXT(PFNGLDETACHSHADERPROC,glDetachShader)
OGLEXT(PFNGLLINKPROGRAMPROC,glLinkProgram)
OGLEXT(PFNGLUSEPROGRAMPROC,glUseProgram)
OGLEXT(PFNGLGETSHADERIVPROC,glGetShaderiv)
OGLEXT(PFNGLGETSHADERINFOLOGPROC,glGetShaderInfoLog)
OGLEXT(PFNGLDELETESHADERPROC,glDeleteShader)
OGLEXT(PFNGLDELETEPROGRAMPROC,glDeleteProgram)
OGLEXT(PFNGLGETPROGRAMIVPROC,glGetProgramiv)
OGLEXT(PFNGLGETPROGRAMINFOLOGPROC,glGetProgramInfoLog)
OGLEXT(PFNGLVALIDATEPROGRAMPROC,glValidateProgram)
OGLEXT(PFNGLBLENDFUNCSEPARATEPROC,glBlendFuncSeparate)
OGLEXT(PFNGLBLENDEQUATIONSEPARATEPROC,glBlendEquationSeparate)
OGLEXT(PFNGLGETUNIFORMLOCATIONPROC,glGetUniformLocation)
OGLEXT(PFNGLUNIFORM1IPROC,glUniform1i)
OGLEXT(PFNGLUNIFORM1IVPROC,glUniform1iv)
OGLEXT(PFNGLUNIFORM1FPROC,glUniform1f)
OGLEXT(PFNGLUNIFORM2FPROC,glUniform2f)
// Generic vertex attributes
OGLEXT(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation)
OGLEXT(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)
OGLEXT(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray)
OGLEXT(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)
// VAO
OGLEXT(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)
OGLEXT(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)
OGLEXT(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)
// VBO and PBO
OGLEXT(PFNGLGENBUFFERSPROC,glGenBuffersARB)
OGLEXT(PFNGLDELETEBUFFERSPROC,glDeleteBuffersARB)
OGLEXT(PFNGLBINDBUFFERPROC,glBindBufferARB)
OGLEXT(PFNGLBUFFERDATAPROC,glBufferDataARB)
OGLEXT(PFNGLBUFFERSUBDATAPROC,glBufferSubDataARB)
OGLEXT(PFNGLMAPBUFFERPROC,glMapBufferARB)
OGLEXT(PFNGLUNMAPBUFFERPROC,glUnmapBufferARB)
// FBO
OGLEXT(PFNGLGENFRAMEBUFFERSEXTPROC,glGenFramebuffersEXT)
OGLEXT(PFNGLBINDFRAMEBUFFEREXTPROC,glBindFramebufferEXT)
OGLEXT(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC,glFramebufferRenderbufferEXT)
OGLEXT(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC,glFramebufferTexture2DEXT)
OGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC,glCheckFramebufferStatusEXT)
OGLEXT(PFNGLDELETEFRAMEBUFFERSEXTPROC,glDeleteFramebuffersEXT)
OGLEXT(PFNGLBLITFRAMEBUFFEREXTPROC,glBlitFramebufferEXT)
// Multisampled FBO
OGLEXT(PFNGLGENRENDERBUFFERSEXTPROC,glGenRenderbuffersEXT)
OGLEXT(PFNGLBINDRENDERBUFFEREXTPROC,glBindRenderbufferEXT)
OGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC,glRenderbufferStorageMultisampleEXT)
OGLEXT(PFNGLDELETERENDERBUFFERSEXTPROC,glDeleteRenderbuffersEXT)
#endif

#if !defined(GL_VERSION_1_3) || defined(_MSC_VER) || defined(__INTEL_COMPILER)
OGLEXT(PFNGLACTIVETEXTUREPROC,glActiveTexture)
#endif

static FORCEINLINE u32 BGRA8888_32_To_RGBA6665_32Rev(const u32 srcPix)
{
	const u32 dstPix = (srcPix >> 2) & 0x3F3F3F3F;
	
	return	 (dstPix & 0x0000FF00) >> 8 |		// R
			 (dstPix & 0x00FF0000) >> 8 |		// G
			 (dstPix & 0xFF000000) >> 8 |		// B
			((dstPix >> 1) & 0x000000FF) << 24;	// A
}

static FORCEINLINE u32 BGRA8888_32Rev_To_RGBA6665_32Rev(const u32 srcPix)
{
	const u32 dstPix = (srcPix >> 2) & 0x3F3F3F3F;
	
	return	 (dstPix & 0x00FF0000) >> 16 |		// R
			 (dstPix & 0x0000FF00)       |		// G
			 (dstPix & 0x000000FF) << 16 |		// B
			((dstPix >> 1) & 0xFF000000);		// A
}

static void OGLConvertFramebuffer(const u32 *__restrict srcBuffer, u32 *dstBuffer)
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
		u32 *__restrict dst = dstBuffer + (y << 8); // Same as dstBuffer + (y * 256)
		
		for(unsigned int x = 0; x < 256; x++, i++)
		{
			// Use the correct endian format since OpenGL uses the native endian of
			// the architecture it is running on.
#ifdef WORDS_BIGENDIAN
			*dst++ = BGRA8888_32_To_RGBA6665_32Rev(srcBuffer[i]);
#else
			*dst++ = BGRA8888_32Rev_To_RGBA6665_32Rev(srcBuffer[i]);
#endif
		}
	}
}

static void* execReadPixelsTask(void *arg)
{
	const unsigned int bufferIndex = *(unsigned int *)arg;
	u32 *__restrict pixBuffer = GPU_screen3D[bufferIndex];
	
	if(!BEGINGL()) return 0;
	
	if (isPBOSupported)
	{
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboRenderDataID[bufferIndex]);
		
		pboRenderBuffer[bufferIndex] = (u32 *__restrict)glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
		if (pboRenderBuffer[bufferIndex] != NULL)
		{
			memcpy(pixBuffer, pboRenderBuffer[bufferIndex], 256 * 192 * sizeof(u32));
			glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
		}
		
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	else
	{
		// Downsample the multisampled FBO to the main framebuffer
		if (selectedRenderingFBO == fboMultisampleRenderID)
		{
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, selectedRenderingFBO);
			glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
			glBlitFramebufferEXT(0, 0, 256, 192, 0, 0, 256, 192, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		}
		
		glReadPixels(0, 0, 256, 192, GL_BGRA, GL_UNSIGNED_BYTE, pixBuffer);
	}
	
	ENDGL();
	
	OGLConvertFramebuffer(pixBuffer, (u32 *)gfx3d_convertedScreen);
	
	return 0;
}

//opengl state caching:
//This is of dubious performance assistance, but it is easy to take out so I am leaving it for now.
//every function that is xgl* can be replaced with gl* if we decide to rip this out or if anyone else
//doesnt feel like sticking with it (or if it causes trouble)

static void xglDepthFunc(GLenum func) {
	static GLenum oldfunc = -1;
	if(oldfunc == func) return;
	glDepthFunc(oldfunc=func);
}

static void xglDepthMask (GLboolean flag) {
	static GLboolean oldflag = -1;
	if(oldflag==flag) return;
	glDepthMask(oldflag=flag);
}

struct GLCaps {
	u8 caps[0x100];
	GLCaps() {
		memset(caps,0xFF,sizeof(caps));
	}
};
static GLCaps glcaps;

static void _xglEnable(GLenum cap) {
	cap -= 0x0B00;
	if(glcaps.caps[cap] == 0xFF || glcaps.caps[cap] == 0) {
		glEnable(cap+0x0B00);
		glcaps.caps[cap] = 1;
	}
}

static void _xglDisable(GLenum cap) {
	cap -= 0x0B00;
	if(glcaps.caps[cap]) {
		glDisable(cap+0x0B00);
		glcaps.caps[cap] = 0;
	}
}

#define xglEnable(cap) { \
	CTASSERT((cap-0x0B00)<0x100); \
	_xglEnable(cap); }

#define xglDisable(cap) {\
	CTASSERT((cap-0x0B00)<0x100); \
	_xglDisable(cap); }

static bool OGLValidateShaderCompile(GLuint theShader)
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

static bool OGLValidateShaderProgramLink(GLuint theProgram)
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

/* Shaders init */

static bool OGLInitShaders(const char *oglExtensionString)
{
#if !defined(GL_ARB_shader_objects)		|| \
	!defined(GL_ARB_vertex_shader)		|| \
	!defined(GL_ARB_fragment_shader)	|| \
	!defined(GL_ARB_vertex_program)
	
	bool isFeatureSupported = false;
#else
	bool isFeatureSupported =  (strstr(oglExtensionString, "GL_ARB_shader_objects") == NULL ||
								strstr(oglExtensionString, "GL_ARB_vertex_shader") == NULL ||
								strstr(oglExtensionString, "GL_ARB_fragment_shader") == NULL ||
								strstr(oglExtensionString, "GL_ARB_vertex_program") == NULL) ? false : true;
#endif
	
#ifdef HAVE_LIBOSMESA
	isFeatureSupported = false;
	INFO("%s\nOpenGL: Shaders aren't supported by OSMesa.\n");
	
#endif
	
	if (!isFeatureSupported)
	{
		INFO("OpenGL: Shaders are unsupported. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return isFeatureSupported;
	}

	vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	if(!vertexShaderID)
	{
		INFO("%s\nOpenGL: Failed to create the vertex shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		
		isFeatureSupported = false;
		return isFeatureSupported;
	}

	glShaderSource(vertexShaderID, 1, (const GLchar**)&vertexShader, NULL);
	glCompileShader(vertexShaderID);
	if (!OGLValidateShaderCompile(vertexShaderID))
	{
		INFO("%s\nOpenGL: Failed to compile the vertex shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		
		glDeleteShader(vertexShaderID);
		
		isFeatureSupported = false;
		return isFeatureSupported;
	}

	fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if(!fragmentShaderID)
	{
		INFO("%s\nOpenGL: Failed to create the fragment shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		
		glDeleteShader(vertexShaderID);
		
		isFeatureSupported = false;
		return isFeatureSupported;
	}

	glShaderSource(fragmentShaderID, 1, (const GLchar**)&fragmentShader, NULL);
	glCompileShader(fragmentShaderID);
	if (!OGLValidateShaderCompile(fragmentShaderID))
	{
		INFO("%s\nOpenGL: Failed to compile the fragment shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		
		glDeleteShader(vertexShaderID);
		glDeleteShader(fragmentShaderID);
		
		isFeatureSupported = false;
		return isFeatureSupported;
	}

	shaderProgram = glCreateProgram();
	if(!shaderProgram)
	{
		INFO("%s\nOpenGL: Failed to create the shader program. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		
		glDeleteShader(vertexShaderID);
		glDeleteShader(fragmentShaderID);
		
		isFeatureSupported = false;
		return isFeatureSupported;
	}

	glAttachShader(shaderProgram, vertexShaderID);
	glAttachShader(shaderProgram, fragmentShaderID);
	
	glBindAttribLocation(shaderProgram, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(shaderProgram, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	glBindAttribLocation(shaderProgram, OGLVertexAttributeID_Color, "inColor");
	
	glLinkProgram(shaderProgram);
	if (!OGLValidateShaderProgramLink(shaderProgram))
	{
		INFO("OpenGL: Failed to link the shader program. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		
		glDetachShader(shaderProgram, vertexShaderID);
		glDetachShader(shaderProgram, fragmentShaderID);
		glDeleteProgram(shaderProgram);
		glDeleteShader(vertexShaderID);
		glDeleteShader(fragmentShaderID);
		
		isFeatureSupported = false;
		return isFeatureSupported;
	}
	
	glValidateProgram(shaderProgram);
	glUseProgram(shaderProgram);

	INFO("OpenGL: Successfully created shaders.\n");
	
	return isFeatureSupported;
}

static bool OGLInitFBOs(const char *oglExtensionString)
{
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
#if	!defined(GL_EXT_framebuffer_object)		|| \
	!defined(GL_EXT_framebuffer_blit)		|| \
	!defined(GL_EXT_packed_depth_stencil)
	
	bool isFeatureSupported = false;
#else
	bool isFeatureSupported = (strstr(oglExtensionString, "GL_EXT_framebuffer_object") == NULL ||
							   strstr(oglExtensionString, "GL_EXT_framebuffer_blit") == NULL ||
							   strstr(oglExtensionString, "GL_EXT_packed_depth_stencil") == NULL) ? false : true;
#endif
	if (!isFeatureSupported)
	{
		INFO("OpenGL: FBOs are unsupported. Some emulation features will be disabled.\n");
		return isFeatureSupported;
	}
	
	// Set up FBO render targets
	glGenTextures(1, &texClearImageColorID);
	glGenTextures(1, &texClearImageDepthStencilID);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ClearImage);
	
	glBindTexture(GL_TEXTURE_2D, texClearImageColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, texClearImageDepthStencilID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, 256, 192, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
	
	glActiveTexture(GL_TEXTURE0);
	
	// Set up FBOs
	glGenFramebuffersEXT(1, &fboClearImageID);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboClearImageID);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texClearImageColorID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, texClearImageDepthStencilID, 0);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to created FBOs. Some emulation features will be disabled.\n");
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &fboClearImageID);
		glDeleteTextures(1, &texClearImageColorID);
		glDeleteTextures(1, &texClearImageDepthStencilID);
		
		isFeatureSupported = false;
		return isFeatureSupported;
	}
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	INFO("OpenGL: Successfully created FBOs.\n");
	
	return isFeatureSupported;
}

static bool OGLInitMultisampledFBO(const char *oglExtensionString)
{
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
#if	!defined(GL_EXT_framebuffer_object)			|| \
	!defined(GL_EXT_framebuffer_multisample)	|| \
	!defined(GL_EXT_framebuffer_blit)			|| \
	!defined(GL_EXT_packed_depth_stencil)
	
	bool isFeatureSupported = false;
#else
	bool isFeatureSupported = (strstr(oglExtensionString, "GL_EXT_framebuffer_object") == NULL ||
							   strstr(oglExtensionString, "GL_EXT_framebuffer_multisample") == NULL ||
							   strstr(oglExtensionString, "GL_EXT_framebuffer_blit") == NULL ||
							   strstr(oglExtensionString, "GL_EXT_packed_depth_stencil") == NULL) ? false : true;
#endif
	if (!isFeatureSupported)
	{
		INFO("OpenGL: Multisampled FBOs are unsupported. Multisample antialiasing will be disabled.\n");
		return isFeatureSupported;
	}
	
	// Check the maximum number of samples that the GPU supports and use that.
	// Since our target resolution is only 256x192 pixels, using the most samples
	// possible is the best thing to do.
	GLint maxSamples = 0;
	glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSamples);
	
	if (maxSamples < 2)
	{
		INFO("OpenGL: GPU does not support at least 2x multisampled FBOs. Multisample antialiasing will be disabled.\n");
		
		isFeatureSupported = false;
		return isFeatureSupported;
	}
	else if (maxSamples > OGLRENDER_MAX_MULTISAMPLES)
	{
		maxSamples = OGLRENDER_MAX_MULTISAMPLES;
	}

	// Set up FBO render targets
	glGenRenderbuffersEXT(1, &rboMultisampleColorID);
	glGenRenderbuffersEXT(1, &rboMultisampleDepthStencilID);
	
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rboMultisampleColorID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, maxSamples, GL_RGBA, 256, 192);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rboMultisampleDepthStencilID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, maxSamples, GL_DEPTH24_STENCIL8_EXT, 256, 192);
	
	// Set up multisampled rendering FBO
	glGenFramebuffersEXT(1, &fboMultisampleRenderID);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboMultisampleRenderID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, rboMultisampleColorID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rboMultisampleDepthStencilID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rboMultisampleDepthStencilID);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to create multisampled FBO. Multisample antialiasing will be disabled.\n");
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &fboMultisampleRenderID);
		glDeleteRenderbuffersEXT(1, &rboMultisampleColorID);
		glDeleteRenderbuffersEXT(1, &rboMultisampleDepthStencilID);
		
		isFeatureSupported = false;
		return isFeatureSupported;
	}
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	INFO("OpenGL: Successfully created multisampled FBO.\n");
	
	return isFeatureSupported;
}

//=================================================

static void OGLReset()
{
	gpuScreen3DHasNewData[0] = false;
	gpuScreen3DHasNewData[1] = false;
	
	if (enableMultithreading)
	{
		for (unsigned int i = 0; i < 2; i++)
		{
			oglReadPixelsTask[i].finish();
		}
	}
	
	if(!BEGINGL())
		return;
	
	glFinish();
	
	for (unsigned int i = 0; i < 2; i++)
	{
		memset(GPU_screen3D[i], 0, sizeof(GPU_screen3D[i]));
	}
	
	if(isShaderSupported)
	{
		hasTexture = false;
		
		glUniform1i(uniformPolyID, 0);
		glUniform1f(uniformPolyAlpha, 1.0f);
		glUniform2f(uniformTexScale, 1.0f, 1.0f);
		glUniform1i(uniformHasTexture, GL_FALSE);
		glUniform1i(uniformPolygonMode, 0);
		glUniform1i(uniformToonShadingMode, 0);
		glUniform1i(uniformWBuffer, 0);
		glUniform1i(uniformEnableAlphaTest, GL_TRUE);
		glUniform1f(uniformAlphaTestRef, 0.0f);
	}
	else
	{
		memset(color4fBuffer, 0, VERTLIST_SIZE * 4 * sizeof(GLfloat));
	}
	
	ENDGL();
	
	memset(vertIndexBuffer, 0, VERT_INDEX_BUFFER_SIZE * sizeof(GLushort));
	currTexture = NULL;
	
	Default3D_Reset();
}

//static class OGLTexCacheUser : public ITexCacheUser
//{
//public:
//	virtual void BindTexture(u32 tx)
//	{
//		glBindTexture(GL_TEXTURE_2D,(GLuint)texcache[tx].id);
//		glMatrixMode (GL_TEXTURE);
//		glLoadIdentity ();
//		glScaled (texcache[tx].invSizeX, texcache[tx].invSizeY, 1.0f);
//
//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//
//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (BIT16(texcache[tx].frm) ? (BIT18(texcache[tx].frm)?GL_MIRRORED_REPEAT:GL_REPEAT) : GL_CLAMP));
//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (BIT17(texcache[tx].frm) ? (BIT19(texcache[tx].frm)?GL_MIRRORED_REPEAT:GL_REPEAT) : GL_CLAMP));
//	}
//
//	virtual void BindTextureData(u32 tx, u8* data)
//	{
//		BindTexture(tx);
//
//	#if 0
//		for (int i=0; i < texcache[tx].sizeX * texcache[tx].sizeY*4; i++)
//			data[i] = 0xFF;
//	#endif
//		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
//			texcache[tx].sizeX, texcache[tx].sizeY, 0, 
//			GL_RGBA, GL_UNSIGNED_BYTE, data);
//	}
//} textures;
//
//static TexCacheUnit texCacheUnit;

static void expandFreeTextures()
{
	const int kInitTextures = 128;
	GLuint oglTempTextureID[kInitTextures];
	glGenTextures(kInitTextures, &oglTempTextureID[0]);
	for(int i=0;i<kInitTextures;i++)
		freeTextureIds.push(oglTempTextureID[i]);
}

static bool OGLIsMinimumVersionSupported(const char *oglVersionString)
{
	bool result = false;
	size_t versionStringLength = 0;
	
	if (oglVersionString == NULL)
	{
		return result;
	}
	
	// First, check for the dot in the revision string. There should be at
	// least one present.
	const char *versionStrEnd = strstr(oglVersionString, ".");
	if (versionStrEnd == NULL)
	{
		return result;
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
	
	if ( (major > OGL_MINIMUM_GPU_VERSION_REQUIRED_MAJOR) ||
		 (major >= OGL_MINIMUM_GPU_VERSION_REQUIRED_MAJOR && minor > OGL_MINIMUM_GPU_VERSION_REQUIRED_MINOR) ||
		 (major >= OGL_MINIMUM_GPU_VERSION_REQUIRED_MAJOR && minor >= OGL_MINIMUM_GPU_VERSION_REQUIRED_MINOR && revision >= OGL_MINIMUM_GPU_VERSION_REQUIRED_REVISION) )
	{
		result = true;
	}
	
	free(versionSubstring);
	versionSubstring = NULL;
	
	return result;
}

static char OGLInit(void)
{
	char result = 0;
	
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
		INFO("OpenGL: Could not initialize -- BEGINGL() failed.");
		result = 0;
		return result;
	}
	
	// Get OpenGL info
	const char *oglVendorString = (const char *)glGetString(GL_VENDOR);
	const char *oglRendererString = (const char *)glGetString(GL_RENDERER);
	const char *oglVersionString = (const char *)glGetString(GL_VERSION);
	
	if (!OGLIsMinimumVersionSupported(oglVersionString))
	{
		INFO("OpenGL: GPU does not support OpenGL v%u.%u.%u or later.\n[GPU Info - Version: %s, Vendor: %s, Renderer: %s]\n",
			 OGL_MINIMUM_GPU_VERSION_REQUIRED_MAJOR, OGL_MINIMUM_GPU_VERSION_REQUIRED_MINOR, OGL_MINIMUM_GPU_VERSION_REQUIRED_REVISION,
			 oglVersionString, oglVendorString, oglRendererString);
		
		result = 0;
		return result;
	}
	
	const char *oglExtensionString = (const char *)glGetString(GL_EXTENSIONS);

	for (u8 i = 0; i < 255; i++)
		material_8bit_to_float[i] = (GLfloat)(i<<2)/255.f;
	
	for (unsigned int i = 0; i < 32768; i++)
		dsDepthToD24S8_LUT[i] = (GLuint)DS_DEPTH15TO24(i) << 8;
	
	expandFreeTextures();
	
	// Maintain our own vertex index buffer for vertex batching and primitive
	// conversions. Such conversions are necessary since OpenGL deprecates
	// primitives like GFX3D_QUADS and GFX3D_QUAD_STRIP in later versions.
	vertIndexBuffer = new GLushort[VERT_INDEX_BUFFER_SIZE];

#ifndef __APPLE__
	INITOGLEXT(PFNGLCREATESHADERPROC,glCreateShader)
	INITOGLEXT(X_PFNGLGETSHADERSOURCEPROC,glShaderSource)
	INITOGLEXT(PFNGLCOMPILESHADERPROC,glCompileShader)
	INITOGLEXT(PFNGLCREATEPROGRAMPROC,glCreateProgram)
	INITOGLEXT(PFNGLATTACHSHADERPROC,glAttachShader)
	INITOGLEXT(PFNGLDETACHSHADERPROC,glDetachShader)
	INITOGLEXT(PFNGLLINKPROGRAMPROC,glLinkProgram)
	INITOGLEXT(PFNGLUSEPROGRAMPROC,glUseProgram)
	INITOGLEXT(PFNGLGETSHADERIVPROC,glGetShaderiv)
	INITOGLEXT(PFNGLGETSHADERINFOLOGPROC,glGetShaderInfoLog)
	INITOGLEXT(PFNGLDELETESHADERPROC,glDeleteShader)
	INITOGLEXT(PFNGLDELETEPROGRAMPROC,glDeleteProgram)
	INITOGLEXT(PFNGLGETPROGRAMIVPROC,glGetProgramiv)
	INITOGLEXT(PFNGLGETPROGRAMINFOLOGPROC,glGetProgramInfoLog)
	INITOGLEXT(PFNGLVALIDATEPROGRAMPROC,glValidateProgram)
	// Generic vertex attributes
	INITOGLEXT(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation)
	INITOGLEXT(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)
	INITOGLEXT(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray)
	INITOGLEXT(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)
	// VAO
	INITOGLEXT(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)
	INITOGLEXT(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)
	INITOGLEXT(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)
	// VBO and PBO
	INITOGLEXT(PFNGLGENBUFFERSPROC,glGenBuffersARB)
	INITOGLEXT(PFNGLDELETEBUFFERSPROC,glDeleteBuffersARB)
	INITOGLEXT(PFNGLBINDBUFFERPROC,glBindBufferARB)
	INITOGLEXT(PFNGLBUFFERDATAPROC,glBufferDataARB)
	INITOGLEXT(PFNGLBUFFERSUBDATAPROC,glBufferSubDataARB)
	INITOGLEXT(PFNGLMAPBUFFERPROC,glMapBufferARB)
	INITOGLEXT(PFNGLUNMAPBUFFERPROC,glUnmapBufferARB)
	// FBO
	INITOGLEXT(PFNGLGENFRAMEBUFFERSEXTPROC,glGenFramebuffersEXT)
	INITOGLEXT(PFNGLBINDFRAMEBUFFEREXTPROC,glBindFramebufferEXT)
	INITOGLEXT(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC,glFramebufferRenderbufferEXT)
	INITOGLEXT(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC,glFramebufferTexture2DEXT)
	INITOGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC,glCheckFramebufferStatusEXT)
	INITOGLEXT(PFNGLDELETEFRAMEBUFFERSEXTPROC,glDeleteFramebuffersEXT)
	INITOGLEXT(PFNGLBLITFRAMEBUFFEREXTPROC,glBlitFramebufferEXT)
	// Multisampled FBO
	INITOGLEXT(PFNGLGENRENDERBUFFERSEXTPROC,glGenRenderbuffersEXT)
	INITOGLEXT(PFNGLBINDRENDERBUFFEREXTPROC,glBindRenderbufferEXT)
	INITOGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC,glRenderbufferStorageMultisampleEXT)
	INITOGLEXT(PFNGLDELETERENDERBUFFERSEXTPROC,glDeleteRenderbuffersEXT)
#ifdef HAVE_LIBOSMESA
	glBlendFuncSeparate = NULL;
#else
	INITOGLEXT(PFNGLBLENDFUNCSEPARATEPROC,glBlendFuncSeparate)
	INITOGLEXT(PFNGLBLENDEQUATIONSEPARATEPROC,glBlendEquationSeparate)
#endif
	INITOGLEXT(PFNGLGETUNIFORMLOCATIONPROC,glGetUniformLocation)
	INITOGLEXT(PFNGLUNIFORM1IPROC,glUniform1i)
	INITOGLEXT(PFNGLUNIFORM1IVPROC,glUniform1iv)
	INITOGLEXT(PFNGLUNIFORM1FPROC,glUniform1f)
	INITOGLEXT(PFNGLUNIFORM2FPROC,glUniform2f)
#endif
#if !defined(GL_VERSION_1_3) || defined(_MSC_VER) || defined(__INTEL_COMPILER)
	INITOGLEXT(PFNGLACTIVETEXTUREPROC,glActiveTexture)
#endif
	
	// VBO Setup
#if !defined(GL_ARB_vertex_buffer_object)
	isVBOSupported = false;
#else
	isVBOSupported = (strstr(oglExtensionString, "GL_ARB_vertex_buffer_object") == NULL) ? false : true;
#endif
	if (isVBOSupported)
	{
		glGenBuffersARB(1, &vboVertexID);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, vboVertexID);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, VERTLIST_SIZE * sizeof(VERT), gfx3d.vertlist, GL_STREAM_DRAW_ARB);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}
	
	// PBO Setup
	// Note that GL_ARB_pixel_buffer_object and GL_EXT_pixel_buffer_object have equivalent
	// tokens, so we'll just use the ARB tokens from here on out.
#if !defined(GL_ARB_pixel_buffer_object) && !defined(GL_EXT_pixel_buffer_object)
	isPBOSupported = false;
#else
	isPBOSupported = (strstr(oglExtensionString, "GL_ARB_pixel_buffer_object") == NULL) ? false : true;
#endif
	if (isPBOSupported)
	{
		glGenBuffersARB(2, pboRenderDataID);
		for (unsigned int i = 0; i < 2; i++)
		{
			glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboRenderDataID[i]);
			glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, 256 * 192 * sizeof(u32), NULL, GL_STREAM_READ_ARB);
			pboRenderBuffer[i] = NULL;
		}
		
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	
	// Render State Setup (common to both shaders and fixed-function pipeline)
	xglEnable(GL_DEPTH_TEST);
	glPixelStorei(GL_PACK_ALIGNMENT, 8);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	//we want to use alpha destination blending so we can track the last-rendered alpha value
	if(glBlendFuncSeparate != NULL)
	{
		if (glBlendEquationSeparate != NULL)
		{
			// test: new super mario brothers renders the stormclouds at the beginning 
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_DST_ALPHA);
			glBlendEquationSeparate( GL_FUNC_ADD, GL_MAX );
		}
		else
		{
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_DST_ALPHA);
		}
	}
	
	// Shader Setup
	isShaderSupported = OGLInitShaders(oglExtensionString);
	if(isShaderSupported)
	{
		// The toon table is a special 1D texture where each pixel corresponds
		// to a specific color in the toon table.
		glGenTextures (1, &texToonTableID);
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ToonTable);
		
		glBindTexture(GL_TEXTURE_1D, texToonTableID);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); //clamp so that we dont run off the edges due to 1.0 -> [0,31] math
		glBindTexture(GL_TEXTURE_1D, 0);
		
		glActiveTexture(GL_TEXTURE0);
		
		memset(currentToonTable16, 0, sizeof(currentToonTable16));
		toonTableNeedsUpdate = true;
		
		// Set up shader uniforms
		GLint uniformTexSampler = glGetUniformLocation(shaderProgram, "texMainRender");
		glUniform1i(uniformTexSampler, 0);
		
		uniformTexSampler = glGetUniformLocation(shaderProgram, "texToonTable");
		glUniform1i(uniformTexSampler, OGLTextureUnitID_ToonTable);
				
		uniformPolyID				= glGetUniformLocation(shaderProgram, "polyID");
		uniformPolyAlpha			= glGetUniformLocation(shaderProgram, "polyAlpha");
		uniformTexScale				= glGetUniformLocation(shaderProgram, "texScale");
		uniformHasTexture			= glGetUniformLocation(shaderProgram, "hasTexture");
		uniformPolygonMode			= glGetUniformLocation(shaderProgram, "polygonMode");
		uniformToonShadingMode		= glGetUniformLocation(shaderProgram, "toonShadingMode");
		uniformWBuffer				= glGetUniformLocation(shaderProgram, "oglWBuffer");
		uniformEnableAlphaTest		= glGetUniformLocation(shaderProgram, "enableAlphaTest");
		uniformAlphaTestRef			= glGetUniformLocation(shaderProgram, "alphaTestRef");
	}
	else
	{
		// Set up fixed-function pipeline render states.
		xglEnable(GL_NORMALIZE);
		glEnable(GL_TEXTURE_1D);
		glEnable(GL_TEXTURE_2D);
		glAlphaFunc(GL_GREATER, 0);
		xglEnable(GL_ALPHA_TEST);
		
		// Map the vertex list's colors with 4 floats per color. This is being done
		// because OpenGL needs 4-colors per vertex to support translucency. (The DS
		// uses 3-colors per vertex, and adds alpha through the poly, so we can't
		// simply reference the colors+alpha from just the vertices themselves.)
		color4fBuffer = new GLfloat[VERTLIST_SIZE * 4];
	}
	
	// VAO Setup
#if !defined(GL_ARB_vertex_array_object) && !defined(GL_APPLE_vertex_array_object)
	isVAOSupported = false;
#else
	isVAOSupported = ( !isVBOSupported ||
					   !isShaderSupported ||
					  (strstr(oglExtensionString, "GL_ARB_vertex_array_object") == NULL &&
					   strstr(oglExtensionString, "GL_APPLE_vertex_array_object") == NULL) ) ? false : true;
#endif
	if (isVAOSupported)
	{
		glGenVertexArrays(1, &vaoMainStatesID);
		glBindVertexArray(vaoMainStatesID);
		
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
		glEnableVertexAttribArray(OGLVertexAttributeID_Color);
		
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, vboVertexID);
		glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
		glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
		
		glBindVertexArray(0);
	}

	// FBO Setup
	isFBOSupported = OGLInitFBOs(oglExtensionString);
	
	// Multisampled FBO Setup
	isMultisampledFBOSupported = OGLInitMultisampledFBO(oglExtensionString);

	ENDGL();
	
	// Multithreading Setup
	if (CommonSettings.num_cores > 1)
	{
#ifdef _WINDOWS
		// Windows doesn't seem like multithreading on the same OpenGL
		// context, so we're disabling this on Windows for now.
		// Someone please research.
		enableMultithreading = false;
#else
		enableMultithreading = true;
		
		for (unsigned int i = 0; i < 2; i++)
		{
			oglReadPixelsTask[i].start(false);
		}
#endif
	}
	else
	{
		enableMultithreading = false;
	}
	
	// Initialization finished -- reset the renderer
	OGLReset();
	INFO("OpenGL: Initialized successfully.\n[GPU Info - Version: %s, Vendor: %s, Renderer: %s]\n",
		 oglVersionString, oglVendorString, oglRendererString);

	return result;
}

static void OGLClose()
{
	gpuScreen3DHasNewData[0] = false;
	gpuScreen3DHasNewData[1] = false;
	
	if (enableMultithreading)
	{
		for (unsigned int i = 0; i < 2; i++)
		{
			oglReadPixelsTask[i].finish();
			oglReadPixelsTask[i].shutdown();
		}
	}
	
	if(!BEGINGL())
		return;
	
	glFinish();
	
	if(isShaderSupported)
	{
		glUseProgram(0);
		
		glDetachShader(shaderProgram, vertexShaderID);
		glDetachShader(shaderProgram, fragmentShaderID);

		glDeleteProgram(shaderProgram);
		glDeleteShader(vertexShaderID);
		glDeleteShader(fragmentShaderID);
		
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ToonTable);
		glBindTexture(GL_TEXTURE_1D, 0);
		glDeleteTextures(1, &texToonTableID);

		isShaderSupported = false;
	}
	else
	{
		delete [] color4fBuffer;
		color4fBuffer = NULL;
	}
	
	if (isVAOSupported)
	{
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &vaoMainStatesID);
		isVAOSupported = false;
	}

	if (isVBOSupported)
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		glDeleteBuffersARB(1, &vboVertexID);
		isVBOSupported = false;
	}
	
	if (isPBOSupported)
	{
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
		glDeleteBuffersARB(2, pboRenderDataID);
		pboRenderBuffer[0] = NULL;
		pboRenderBuffer[1] = NULL;
		isPBOSupported = false;
	}
	
	// FBO
	if (isFBOSupported)
	{
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ClearImage);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		
		glDeleteFramebuffersEXT(1, &fboClearImageID);
		glDeleteTextures(1, &texClearImageColorID);
		glDeleteTextures(1, &texClearImageDepthStencilID);
		
		isFBOSupported = false;
	}
	
	if (isMultisampledFBOSupported)
	{
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &fboMultisampleRenderID);
		glDeleteRenderbuffersEXT(1, &rboMultisampleColorID);
		glDeleteRenderbuffersEXT(1, &rboMultisampleDepthStencilID);
		
		selectedRenderingFBO = 0;
		isMultisampledFBOSupported = false;
	}
	
	//kill the tex cache to free all the texture ids
	TexCache_Reset();
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	while(!freeTextureIds.empty())
	{
		GLuint temp = freeTextureIds.front();
		freeTextureIds.pop();
		glDeleteTextures(1,&temp);
	}
	
	hasTexture = false;
	currTexture = NULL;
	
	glFinish();
	
	ENDGL();
	
	delete [] vertIndexBuffer;
	vertIndexBuffer = NULL;
	
	Default3D_Close();
}

static void texDeleteCallback(TexCacheItem* item)
{
	freeTextureIds.push((GLuint)item->texid);
	if(currTexture == item)
		currTexture = NULL;
}

static void SetupTexture(const POLY *thePoly)
{
	PolygonTexParams params = thePoly->getTexParams();
	
	// Check if we need to use textures
	if (thePoly->texParam == 0 || params.texFormat == TEXMODE_NONE || !gfx3d.renderState.enableTexturing)
	{
		if (hasTexture)
		{
			hasTexture = false;
			glActiveTexture(GL_TEXTURE0);
			
			if (isShaderSupported)
			{
				glUniform1i(uniformHasTexture, GL_FALSE);
			}
			else
			{
				glDisable(GL_TEXTURE_2D);
			}
		}
		
		return;
	}
	
	// Enable textures if they weren't already enabled
	if (!hasTexture)
	{
		hasTexture = true;
		glActiveTexture(GL_TEXTURE0);
		
		if (isShaderSupported)
		{
			glUniform1i(uniformHasTexture, GL_TRUE);
		}
		else
		{
			glEnable(GL_TEXTURE_2D);
		}
	}
	
//	texCacheUnit.TexCache_SetTexture<TexFormat_32bpp>(format, texpal);
	TexCacheItem* newTexture = TexCache_SetTexture(TexFormat_32bpp, thePoly->texParam, thePoly->texPalette);
	if(newTexture != currTexture)
	{
		currTexture = newTexture;
		//has the ogl renderer initialized the texture?
		if(!currTexture->deleteCallback)
		{
			currTexture->deleteCallback = texDeleteCallback;
			if(freeTextureIds.empty()) expandFreeTextures();
			currTexture->texid = (u64)freeTextureIds.front();
			freeTextureIds.pop();

			glBindTexture(GL_TEXTURE_2D,(GLuint)currTexture->texid);
			
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (params.enableRepeatS ? (params.enableMirroredRepeatS ? GL_MIRRORED_REPEAT : GL_REPEAT) : GL_CLAMP_TO_EDGE));
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (params.enableRepeatT ? (params.enableMirroredRepeatT ? GL_MIRRORED_REPEAT : GL_REPEAT) : GL_CLAMP_TO_EDGE));
			
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
				currTexture->sizeX, currTexture->sizeY, 0, 
				GL_RGBA, GL_UNSIGNED_BYTE, currTexture->decoded);
		}
		else
		{
			//otherwise, just bind it
			glBindTexture(GL_TEXTURE_2D,(GLuint)currTexture->texid);
		}

		//in either case, we need to setup the tex mtx
		if (isShaderSupported)
		{
			glUniform2f(uniformTexScale, currTexture->invSizeX, currTexture->invSizeY);
		}
		else
		{
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();
			glScalef(currTexture->invSizeX, currTexture->invSizeY, 1.0f);
		}
	}
}

static void SetupPolygon(const POLY *thePoly)
{
	static unsigned int lastTexBlendMode = 0;
	static int lastStencilState = -1;
	
	PolygonAttributes attr = thePoly->getAttributes();
	
	// Set up polygon ID
	if (isShaderSupported)
	{
		glUniform1i(uniformPolyID, attr.polygonID);
	}
	
	// Set up alpha value
	polyAlpha = 1.0f;
	if (!attr.isWireframe && attr.isTranslucent)
	{
		polyAlpha = divide5bitBy31LUT[attr.alpha];
	}
	
	if (isShaderSupported)
	{
		glUniform1f(uniformPolyAlpha, polyAlpha);
	}
	
	// Set up depth test mode
	static const GLenum oglDepthFunc[2] = {GL_LESS, GL_EQUAL};
	xglDepthFunc(oglDepthFunc[attr.enableDepthTest]);

	// Set up culling mode
	static const GLenum oglCullingMode[4] = {GL_FRONT_AND_BACK, GL_FRONT, GL_BACK, 0};
	GLenum cullingMode = oglCullingMode[attr.surfaceCullingMode];
	
	if (cullingMode == 0)
	{
		xglDisable(GL_CULL_FACE);
	}
	else
	{
		xglEnable(GL_CULL_FACE);
		glCullFace(cullingMode);
	}
	
	// Set up depth write
	GLboolean enableDepthWrite = GL_TRUE;
	
	// Handle shadow polys. Do this after checking for depth write, since shadow polys
	// can change this too.
	if(attr.polygonMode == 3)
	{
		xglEnable(GL_STENCIL_TEST);
		if(attr.polygonID == 0)
		{
			enableDepthWrite = GL_FALSE;
			if(lastStencilState != 0)
			{
				lastStencilState = 0;
				//when the polyID is zero, we are writing the shadow mask.
				//set stencilbuf = 1 where the shadow volume is obstructed by geometry.
				//do not write color or depth information.
				glStencilFunc(GL_ALWAYS,65,255);
				glStencilOp(GL_KEEP,GL_REPLACE,GL_KEEP);
				glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
			}
		}
		else
		{
			enableDepthWrite = GL_TRUE;
			if(lastStencilState != 1)
			{
				lastStencilState = 1;
				//when the polyid is nonzero, we are drawing the shadow poly.
				//only draw the shadow poly where the stencilbuf==1.
				//I am not sure whether to update the depth buffer here--so I chose not to.
				glStencilFunc(GL_EQUAL,65,255);
				glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
				glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
			}
		}
	}
	else
	{
		xglEnable(GL_STENCIL_TEST);
		if(attr.isTranslucent)
		{
			lastStencilState = 3;
			glStencilFunc(GL_NOTEQUAL,attr.polygonID,255);
			glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
			glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
		}
		else if(lastStencilState != 2)
		{
			lastStencilState = 2;	
			glStencilFunc(GL_ALWAYS,64,255);
			glStencilOp(GL_REPLACE,GL_REPLACE,GL_REPLACE);
			glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
		}
	}
	
	if(attr.isTranslucent && !attr.enableAlphaDepthWrite)
	{
		enableDepthWrite = GL_FALSE;
	}
	
	xglDepthMask(enableDepthWrite);
	
	// Set up texture blending mode
	if(attr.polygonMode != lastTexBlendMode)
	{
		lastTexBlendMode = attr.polygonMode;
		
		if(isShaderSupported)
		{
			glUniform1i(uniformPolygonMode, attr.polygonMode);
			
			// Update the toon table if necessary
			if (toonTableNeedsUpdate && attr.polygonMode == 2)
			{
				glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ToonTable);
				glBindTexture(GL_TEXTURE_1D, texToonTableID);
				glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, currentToonTable16);
				glActiveTexture(GL_TEXTURE0);
				
				toonTableNeedsUpdate = false;
			}
		}
		else
		{
			static const GLint oglTexBlendMode[4] = {GL_MODULATE, GL_DECAL, GL_MODULATE, GL_MODULATE};
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, oglTexBlendMode[attr.polygonMode]);
		}
	}
}

static void SetupViewport(const POLY *thePoly)
{
	VIEWPORT viewport;
	viewport.decode(thePoly->viewport);
	glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
}

static void Control()
{
	if (isShaderSupported)
	{
		if(gfx3d.renderState.enableAlphaTest)
		{
			glUniform1i(uniformEnableAlphaTest, GL_TRUE);
		}
		else
		{
			glUniform1i(uniformEnableAlphaTest, GL_FALSE);
		}
		
		glUniform1f(uniformAlphaTestRef, divide5bitBy31LUT[gfx3d.renderState.alphaTestRef]);
		glUniform1i(uniformToonShadingMode, gfx3d.renderState.shading);
	}
	else
	{
		if(gfx3d.renderState.enableAlphaTest && (gfx3d.renderState.alphaTestRef > 0))
		{
			glAlphaFunc(GL_GEQUAL, divide5bitBy31LUT[gfx3d.renderState.alphaTestRef]);
		}
		else
		{
			glAlphaFunc(GL_GREATER, 0);
		}
	}
	
	if(gfx3d.renderState.enableAlphaBlending)
	{
		glEnable(GL_BLEND);
	}
	else
	{
		glDisable(GL_BLEND);
	}
	
	if (isMultisampledFBOSupported && CommonSettings.GFX3D_Renderer_Multisample)
	{
		selectedRenderingFBO = fboMultisampleRenderID;
	}
	else
	{
		selectedRenderingFBO = 0;
	}
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, selectedRenderingFBO);
}

static void GL_ReadFramebuffer()
{
	static unsigned int bufferIndex = 0;
	
	bufferIndex = (bufferIndex + 1) & 0x01;
	gpuScreen3DBufferIndex = bufferIndex;
	
	if (enableMultithreading)
	{
		oglReadPixelsTask[bufferIndex].finish();
	}
	
	if (isPBOSupported)
	{
		if(!BEGINGL()) return;
		
		// Downsample the multisampled FBO to the main framebuffer
		if (selectedRenderingFBO == fboMultisampleRenderID)
		{
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, selectedRenderingFBO);
			glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
			glBlitFramebufferEXT(0, 0, 256, 192, 0, 0, 256, 192, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		}
		
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboRenderDataID[bufferIndex]);
		glReadPixels(0, 0, 256, 192, GL_BGRA, GL_UNSIGNED_BYTE, 0);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
		
		ENDGL();
	}
	
	// If multithreading is ENABLED, call glReadPixels() on a separate thread
	// (or glMapBuffer()/glUnmapBuffer() if PBOs are supported). This is a big
	// deal, since these functions can cause the thread to block. If 3D rendering
	// is happening on the same thread as the core emulation, (which is the most
	// likely scenario), this can make the thread stall.
	//
	// We can get away with doing this since 3D rendering begins on H-Start,
	// but the emulation doesn't actually need the rendered data until H-Blank.
	// So in between that time, we can let these functions block the other thread
	// and then only block this thread for the remaining time difference.
	//
	// But if multithreading is DISABLED, then we defer our pixel reading until
	// H-Blank, and let that logic determine whether a pixel read is needed or not.
	// This can save us some time in cases where games don't require the 3D layer
	// for this particular frame.
	
	gpuScreen3DHasNewData[bufferIndex] = true;
	
	if (enableMultithreading)
	{
		oglReadPixelsTask[bufferIndex].execute(&execReadPixelsTask, &bufferIndex);
	}
}

// Tested:	Sonic Chronicles Dark Brotherhood
//			The Chronicles of Narnia - The Lion, The Witch and The Wardrobe
//			Harry Potter and the Order of the Phoenix
static void HandleClearImage()
{
	static const size_t pixelsPerLine = 256;
	static const size_t lineCount = 192;
	static const size_t totalPixelCount = pixelsPerLine * lineCount;
	static const size_t bufferSize = totalPixelCount * sizeof(u16);
	
	static CACHE_ALIGN GLushort oglClearImageColor[totalPixelCount] = {0};
	static CACHE_ALIGN GLint oglClearImageDepth[totalPixelCount] = {0};
	static CACHE_ALIGN u16 oglClearImageColorTemp[totalPixelCount] = {0};
	static CACHE_ALIGN u16 oglClearImageDepthTemp[totalPixelCount] = {0};
	static u16 lastScroll = 0;
	
	if (!isFBOSupported)
	{
		return;
	}
	
	const u16 *__restrict clearImage = (u16 *__restrict)MMU.texInfo.textureSlotAddr[2];
	const u16 *__restrict clearDepth = (u16 *__restrict)MMU.texInfo.textureSlotAddr[3];
	const u16 scroll = T1ReadWord(MMU.ARM9_REG, 0x356); //CLRIMAGE_OFFSET

	if (lastScroll != scroll ||
		memcmp(clearImage, oglClearImageColorTemp, bufferSize) ||
		memcmp(clearDepth, oglClearImageDepthTemp, bufferSize) )
	{
		lastScroll = scroll;
		memcpy(oglClearImageColorTemp, clearImage, bufferSize);
		memcpy(oglClearImageDepthTemp, clearDepth, bufferSize);
		
		const u16 xScroll = scroll & 0xFF;
		const u16 yScroll = (scroll >> 8) & 0xFF;
		unsigned int dd = totalPixelCount - pixelsPerLine;
		
		for(unsigned int iy = 0; iy < lineCount; iy++) 
		{
			const unsigned int y = ((iy + yScroll) & 0xFF) << 8;
			
			for(unsigned int ix = 0; ix < pixelsPerLine; ix++)
			{
				const unsigned int x = (ix + xScroll) & 0xFF;
				const unsigned int adr = y + x;
				
				oglClearImageColor[dd] = clearImage[adr];
				oglClearImageDepth[dd] = dsDepthToD24S8_LUT[clearDepth[adr] & 0x7FFF];
				
				dd++;
			}
			
			dd -= pixelsPerLine * 2;
		}
		
		// Upload color pixels and depth buffer
		glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ClearImage);
		
		glBindTexture(GL_TEXTURE_2D, texClearImageColorID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pixelsPerLine, lineCount, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, oglClearImageColor);
		glBindTexture(GL_TEXTURE_2D, texClearImageDepthStencilID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pixelsPerLine, lineCount, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, oglClearImageDepth);
		glBindTexture(GL_TEXTURE_2D, 0);
		
		glActiveTexture(GL_TEXTURE0);
	}
	
	// Copy the clear image to the main framebuffer
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, fboClearImageID);
	glBlitFramebufferEXT(0, 0, pixelsPerLine, lineCount, 0, 0, pixelsPerLine, lineCount, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, selectedRenderingFBO);
}

static void OGLRender()
{
	if(!BEGINGL()) return;

	Control();

	if(isShaderSupported)
	{
		// Update the toon table if it changed.
		if (memcmp(currentToonTable16, gfx3d.renderState.u16ToonTable, sizeof(currentToonTable16)))
		{
			memcpy(currentToonTable16, gfx3d.renderState.u16ToonTable, sizeof(currentToonTable16));
			toonTableNeedsUpdate = true;
		}
		
		glUniform1i(uniformWBuffer, gfx3d.renderState.wbuffer);
	}

	xglDepthMask(GL_TRUE);

	glClearStencil((gfx3d.renderState.clearColor >> 24) & 0x3F);
	u32 clearFlag = GL_STENCIL_BUFFER_BIT;

	if (isFBOSupported && gfx3d.renderState.enableClearImage)
	{
		HandleClearImage();
	}
	else
	{
		GLfloat clearColor[4] = {
			divide5bitBy31LUT[gfx3d.renderState.clearColor & 0x1F],
			divide5bitBy31LUT[(gfx3d.renderState.clearColor >> 5) & 0x1F],
			divide5bitBy31LUT[(gfx3d.renderState.clearColor >> 10) & 0x1F],
			divide5bitBy31LUT[(gfx3d.renderState.clearColor >> 16) & 0x1F]
		};
		
		glClearColor(clearColor[0],clearColor[1],clearColor[2],clearColor[3]);
		glClearDepth((GLfloat)gfx3d.renderState.clearDepth / (GLfloat)0x00FFFFFF);
		clearFlag |= GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
	}
	
	glClear(clearFlag);

	if (!isShaderSupported)
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
	}

	//render display list
	//TODO - properly doublebuffer the display lists
	{
		u32 lastTexParams = 0;
		u32 lastTexPalette = 0;
		u32 lastPolyAttr = 0;
		u32 lastViewport = 0xFFFFFFFF;
		unsigned int polyCount = gfx3d.polylist->count;
		unsigned int vertIndexCount = 0;
		GLenum polyPrimitive = 0;
		unsigned int polyType = 0;
		bool needVertexUpload = true;
		
		// Assign vertex attributes based on which OpenGL features we have.
		if (isVAOSupported)
		{
			glBindVertexArray(vaoMainStatesID);
			glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VERT) * gfx3d.vertlist->count, gfx3d.vertlist);
		}
		else
		{
			if (isShaderSupported)
			{
				glEnableVertexAttribArray(OGLVertexAttributeID_Position);
				glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
				glEnableVertexAttribArray(OGLVertexAttributeID_Color);
				
				if (isVBOSupported)
				{
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, vboVertexID);
					glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VERT) * gfx3d.vertlist->count, gfx3d.vertlist);
					glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
					glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
					glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
				}
				else
				{
					glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), &gfx3d.vertlist->list[0].coord);
					glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), &gfx3d.vertlist->list[0].texcoord);
					glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), &gfx3d.vertlist->list[0].color);
				}
			}
			else
			{
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glEnableClientState(GL_COLOR_ARRAY);
				glEnableClientState(GL_VERTEX_ARRAY);
				
				if (isVBOSupported)
				{
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
					glColorPointer(4, GL_FLOAT, 0, color4fBuffer);
					
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, vboVertexID);
					glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VERT) * gfx3d.vertlist->count, gfx3d.vertlist);
					glVertexPointer(4, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
					glTexCoordPointer(2, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
				}
				else
				{
					glVertexPointer(4, GL_FLOAT, sizeof(VERT), &gfx3d.vertlist->list[0].coord);
					glTexCoordPointer(2, GL_FLOAT, sizeof(VERT), &gfx3d.vertlist->list[0].texcoord);
					glColorPointer(4, GL_FLOAT, 0, color4fBuffer);
				}
			}
		}
		
		// Map GFX3D_QUADS and GFX3D_QUAD_STRIP to GL_TRIANGLES since we will convert them.
		//
		// Also map GFX3D_TRIANGLE_STRIP to GL_TRIANGLES. This is okay since this is actually
		// how the POLY struct stores triangle strip vertices, which is in sets of 3 vertices
		// each. This redefinition is necessary since uploading more than 3 indices at a time
		// will cause glDrawElements() to draw the triangle strip incorrectly.
		static const GLenum oglPrimitiveType[]	= {GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES,
												   GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_STRIP, GL_LINE_STRIP};
		
		// Render all polygons
		for(unsigned int i = 0; i < polyCount; i++)
		{
			const POLY *poly = &gfx3d.polylist->list[gfx3d.indexlist.list[i]];
			polyPrimitive = oglPrimitiveType[poly->vtxFormat];
			polyType = poly->type;

			// Set up the polygon if it changed
			if(lastPolyAttr != poly->polyAttr || i == 0)
			{
				lastPolyAttr = poly->polyAttr;
				SetupPolygon(poly);
			}
			
			// Set up the texture if it changed
			if (lastTexParams != poly->texParam || lastTexPalette != poly->texPalette || i == 0)
			{
				lastTexParams = poly->texParam;
				lastTexPalette = poly->texPalette;
				SetupTexture(poly);
			}
			
			// Set up the viewport if it changed
			if(lastViewport != poly->viewport || i == 0)
			{
				lastViewport = poly->viewport;
				SetupViewport(poly);
			}
			
			// Set up vertices
			if (isShaderSupported)
			{
				for(unsigned int j = 0; j < polyType; j++)
				{
					const GLushort vertIndex = poly->vertIndexes[j];
					
					// While we're looping through our vertices, add each vertex index to
					// a buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
					// vertices here to convert them to GL_TRIANGLES, which are much easier
					// to work with and won't be deprecated in future OpenGL versions.
					vertIndexBuffer[vertIndexCount++] = vertIndex;
					if (poly->vtxFormat == GFX3D_QUADS || poly->vtxFormat == GFX3D_QUAD_STRIP)
					{
						if (j == 2)
						{
							vertIndexBuffer[vertIndexCount++] = vertIndex;
						}
						else if (j == 3)
						{
							vertIndexBuffer[vertIndexCount++] = poly->vertIndexes[0];
						}
					}
				}
			}
			else
			{
				for(unsigned int j = 0; j < polyType; j++)
				{
					const GLushort vertIndex = poly->vertIndexes[j];
					const GLushort colorIndex = vertIndex * 4;
					
					// Consolidate the vertex color and the poly alpha to our internal color buffer
					// so that OpenGL can use it.
					VERT *vert = &gfx3d.vertlist->list[vertIndex];
					color4fBuffer[colorIndex+0] = material_8bit_to_float[vert->color[0]];
					color4fBuffer[colorIndex+1] = material_8bit_to_float[vert->color[1]];
					color4fBuffer[colorIndex+2] = material_8bit_to_float[vert->color[2]];
					color4fBuffer[colorIndex+3] = polyAlpha;
					
					// While we're looping through our vertices, add each vertex index to a
					// buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
					// vertices here to convert them to GL_TRIANGLES, which are much easier
					// to work with and won't be deprecated in future OpenGL versions.
					vertIndexBuffer[vertIndexCount++] = vertIndex;
					if (poly->vtxFormat == GFX3D_QUADS || poly->vtxFormat == GFX3D_QUAD_STRIP)
					{
						if (j == 2)
						{
							vertIndexBuffer[vertIndexCount++] = vertIndex;
						}
						else if (j == 3)
						{
							vertIndexBuffer[vertIndexCount++] = poly->vertIndexes[0];
						}
					}
				}
			}
			
			needVertexUpload = true;
			
			// Look ahead to the next polygon to see if we can simply buffer the indices
			// instead of uploading them now. We can buffer if all polygon states remain
			// the same and we're not drawing a line loop or line strip.
			if (i+1 < polyCount)
			{
				const POLY *nextPoly = &gfx3d.polylist->list[gfx3d.indexlist.list[i+1]];
				
				if (lastPolyAttr == nextPoly->polyAttr &&
					lastTexParams == nextPoly->texParam &&
					lastTexPalette == nextPoly->texPalette &&
					polyPrimitive == oglPrimitiveType[nextPoly->vtxFormat] &&
					polyPrimitive != GL_LINE_LOOP &&
					polyPrimitive != GL_LINE_STRIP &&
					oglPrimitiveType[nextPoly->vtxFormat] != GL_LINE_LOOP &&
					oglPrimitiveType[nextPoly->vtxFormat] != GL_LINE_STRIP)
				{
					needVertexUpload = false;
				}
			}
			
			// Upload the vertices if necessary.
			if (needVertexUpload)
			{
				// In wireframe mode, redefine all primitives as GL_LINE_LOOP rather than
				// setting the polygon mode to GL_LINE though glPolygonMode(). Not only is
				// drawing more accurate this way, but it also allows GFX3D_QUADS and
				// GFX3D_QUAD_STRIP primitives to properly draw as wireframe without the
				// extra diagonal line.
				if (poly->isWireframe())
				{
					polyPrimitive = GL_LINE_LOOP;
				}
				
				// Upload the vertices to the framebuffer.
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, vertIndexBuffer);
				vertIndexCount = 0;
			}
		}
		
		// Disable vertex attributes.
		if (isVAOSupported)
		{
			glBindVertexArray(0);
		}
		else
		{
			if (isShaderSupported)
			{
				if (isVBOSupported)
				{
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
				}
				
				glDisableVertexAttribArray(OGLVertexAttributeID_Position);
				glDisableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
				glDisableVertexAttribArray(OGLVertexAttributeID_Color);
			}
			else
			{
				if (isVBOSupported)
				{
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
				}
				
				glDisableClientState(GL_VERTEX_ARRAY);
				glDisableClientState(GL_COLOR_ARRAY);
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			}
		}
	}
	
	//needs to happen before endgl because it could free some textureids for expired cache items
	TexCache_EvictFrame();

	ENDGL();

	GL_ReadFramebuffer();
}

static void OGLVramReconfigureSignal()
{
	Default3D_VramReconfigureSignal();
}

static void OGLRenderFinish()
{
	// If OpenGL is still reading back pixels on a separate thread, wait for it to finish.
	// Otherwise, just do the pixel read now.
	if (gpuScreen3DHasNewData[gpuScreen3DBufferIndex])
	{
		if (enableMultithreading)
		{
			oglReadPixelsTask[gpuScreen3DBufferIndex].finish();
		}
		else
		{
			execReadPixelsTask(&gpuScreen3DBufferIndex);
		}
		
		gpuScreen3DHasNewData[gpuScreen3DBufferIndex] = false;
	}
}

GPU3DInterface gpu3Dgl = {
	"OpenGL",
	OGLInit,
	OGLReset,
	OGLClose,
	OGLRender,
	OGLRenderFinish,
	OGLVramReconfigureSignal
};
