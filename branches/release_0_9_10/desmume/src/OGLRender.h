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
EXTERNOGLEXT(PFNGLUNIFORM2FPROC, glUniform2f) // Core in v2.0
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
EXTERNOGLEXT(PFNGLUNIFORM2FPROC, glUniform2f) // Core in v2.0
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

#endif // OGLRENDER_3_2_H

// Define the minimum required OpenGL version for the driver to support
#define OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MAJOR			1
#define OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MINOR			2
#define OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_REVISION		0

#define OGLRENDER_MAX_MULTISAMPLES			16
#define OGLRENDER_VERT_INDEX_BUFFER_COUNT	131072

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

enum OGLErrorCode
{
	OGLERROR_NOERR = RENDER3DERROR_NOERR,
	
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

struct OGLRenderRef
{	
	// OpenGL Feature Support
	GLint stateTexMirroredRepeat;
	
	// VBO
	GLuint vboVertexID;
	GLuint iboIndexID;
	
	// PBO
	GLuint pboRenderDataID[2];
	
	// FBO
	GLuint texClearImageColorID;
	GLuint texClearImageDepthStencilID;
	GLuint fboClearImageID;
	
	GLuint rboFinalOutputColorID;
	GLuint rboFinalOutputDepthStencilID;
	GLuint fboFinalOutputID;
	
	// Multisampled FBO
	GLuint rboMultisampleColorID;
	GLuint rboMultisampleDepthStencilID;
	GLuint fboMultisampleRenderID;
	GLuint selectedRenderingFBO;
	
	// Shader states
	GLuint vertexShaderID;
	GLuint fragmentShaderID;
	GLuint shaderProgram;
	
	GLint uniformPolyID;
	GLint uniformPolyAlpha;
	GLint uniformTexScale;
	GLint uniformHasTexture;
	GLint uniformPolygonMode;
	GLint uniformToonShadingMode;
	GLint uniformWBuffer;
	GLint uniformEnableAlphaTest;
	GLint uniformAlphaTestRef;
	
	GLuint texToonTableID;
	
	// VAO
	GLuint vaoMainStatesID;
	
	// Textures
	std::queue<GLuint> freeTextureIDs;
	
	// Client-side Buffers
	GLfloat *color4fBuffer;
	DS_ALIGN(16) GLushort vertIndexBuffer[OGLRENDER_VERT_INDEX_BUFFER_COUNT];
};

struct GFX3D_State;
struct VERTLIST;
struct POLYLIST;
struct INDEXLIST;
struct POLY;
class TexCacheItem;
class OpenGLRenderer;

extern GPU3DInterface gpu3Dgl;
extern GPU3DInterface gpu3DglOld;
extern GPU3DInterface gpu3Dgl_3_2;

//This is called by OGLRender whenever it initializes.
//Platforms, please be sure to set this up.
//return true if you successfully init.
extern bool (*oglrender_init)();

//This is called by OGLRender before it uses opengl.
//return true if youre OK with using opengl
extern bool (*oglrender_beginOpenGL)();

//This is called by OGLRender after it is done using opengl.
extern void (*oglrender_endOpenGL)();

// These functions need to be assigned by ports that support using an
// OpenGL 3.2 Core Profile context. The OGLRender_3_2.cpp file includes
// the corresponding functions to assign to each function pointer.
//
// If any of these functions are unassigned, then one of the legacy OpenGL
// renderers will be used instead.
extern void (*OGLLoadEntryPoints_3_2_Func)();
extern void (*OGLCreateRenderer_3_2_Func)(OpenGLRenderer **rendererPtr);

// Lookup Tables
extern CACHE_ALIGN GLfloat material_8bit_to_float[256];
extern CACHE_ALIGN GLuint dsDepthToD24S8_LUT[32768];
extern const GLfloat divide5bitBy31_LUT[32];

FORCEINLINE u32 BGRA8888_32_To_RGBA6665_32(const u32 srcPix);
FORCEINLINE u32 BGRA8888_32Rev_To_RGBA6665_32Rev(const u32 srcPix);
bool IsVersionSupported(unsigned int checkVersionMajor, unsigned int checkVersionMinor, unsigned int checkVersionRevision);

class OpenGLRenderer : public Render3D
{
private:
	// Driver's OpenGL Version
	unsigned int versionMajor;
	unsigned int versionMinor;
	unsigned int versionRevision;
	
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
	
	// Textures
	TexCacheItem *currTexture;
	
	u16 currentToonTable16[32];
	bool toonTableNeedsUpdate;
	
	DS_ALIGN(16) u32 GPU_screen3D[2][256 * 192 * sizeof(u32)];
	bool gpuScreen3DHasNewData[2];
	unsigned int doubleBufferIndex;
	u8 clearImageStencilValue;
	
	// OpenGL-specific methods
	virtual Render3DError CreateVBOs() = 0;
	virtual void DestroyVBOs() = 0;
	virtual Render3DError CreatePBOs() = 0;
	virtual void DestroyPBOs() = 0;
	virtual Render3DError CreateFBOs() = 0;
	virtual void DestroyFBOs() = 0;
	virtual Render3DError CreateMultisampledFBO() = 0;
	virtual void DestroyMultisampledFBO() = 0;
	virtual Render3DError CreateShaders(const std::string *vertexShaderProgram, const std::string *fragmentShaderProgram) = 0;
	virtual void DestroyShaders() = 0;
	virtual Render3DError CreateVAOs() = 0;
	virtual void DestroyVAOs() = 0;
	virtual Render3DError InitTextures() = 0;
	virtual Render3DError InitFinalRenderStates(const std::set<std::string> *oglExtensionSet) = 0;
	virtual Render3DError InitTables() = 0;
	
	virtual Render3DError LoadShaderPrograms(std::string *outVertexShaderProgram, std::string *outFragmentShaderProgram) = 0;
	virtual Render3DError SetupShaderIO() = 0;
	virtual Render3DError CreateToonTable() = 0;
	virtual Render3DError DestroyToonTable() = 0;
	virtual Render3DError UploadToonTable(const GLushort *toonTableBuffer) = 0;
	virtual Render3DError CreateClearImage() = 0;
	virtual Render3DError DestroyClearImage() = 0;
	virtual Render3DError UploadClearImage(const GLushort *clearImageColorBuffer, const GLint *clearImageDepthBuffer) = 0;
	
	virtual void GetExtensionSet(std::set<std::string> *oglExtensionSet) = 0;
	virtual Render3DError ExpandFreeTextures() = 0;
	virtual Render3DError SetupVertices(const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList, GLushort *outIndexBuffer, unsigned int *outIndexCount) = 0;
	virtual Render3DError EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const unsigned int vertIndexCount) = 0;
	virtual Render3DError DisableVertexAttributes() = 0;
	virtual Render3DError SelectRenderingFramebuffer() = 0;
	virtual Render3DError DownsampleFBO() = 0;
	virtual Render3DError ReadBackPixels() = 0;
	
	// Base rendering methods
	virtual Render3DError BeginRender(const GFX3D_State *renderState) = 0;
	virtual Render3DError PreRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList) = 0;
	virtual Render3DError DoRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList) = 0;
	virtual Render3DError PostRender() = 0;
	virtual Render3DError EndRender(const u64 frameCount) = 0;
	
	virtual Render3DError UpdateClearImage(const u16 *__restrict colorBuffer, const u16 *__restrict depthBuffer, const u8 clearStencil, const u8 xScroll, const u8 yScroll) = 0;
	virtual Render3DError UpdateToonTable(const u16 *toonTableBuffer) = 0;
	
	virtual Render3DError ClearUsingImage() const = 0;
	virtual Render3DError ClearUsingValues(const u8 r, const u8 g, const u8 b, const u8 a, const u32 clearDepth, const u8 clearStencil) const = 0;
	
	virtual Render3DError SetupPolygon(const POLY *thePoly) = 0;
	virtual Render3DError SetupTexture(const POLY *thePoly, bool enableTexturing) = 0;
	virtual Render3DError SetupViewport(const POLY *thePoly) = 0;
	
public:
	OpenGLRenderer();
	virtual ~OpenGLRenderer() {};
	
	virtual Render3DError InitExtensions() = 0;
	virtual Render3DError Reset() = 0;
	virtual Render3DError RenderFinish() = 0;
	
	virtual Render3DError DeleteTexture(const TexCacheItem *item) = 0;
	
	bool IsExtensionPresent(const std::set<std::string> *oglExtensionSet, const std::string extensionName) const;
	bool ValidateShaderCompile(GLuint theShader) const;
	bool ValidateShaderProgramLink(GLuint theProgram) const;
	void GetVersion(unsigned int *major, unsigned int *minor, unsigned int *revision) const;
	void SetVersion(unsigned int major, unsigned int minor, unsigned int revision);
	void ConvertFramebuffer(const u32 *__restrict srcBuffer, u32 *dstBuffer);
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
	virtual Render3DError CreateMultisampledFBO();
	virtual void DestroyMultisampledFBO();
	virtual Render3DError CreateShaders(const std::string *vertexShaderProgram, const std::string *fragmentShaderProgram);
	virtual void DestroyShaders();
	virtual Render3DError CreateVAOs();
	virtual void DestroyVAOs();
	virtual Render3DError InitTextures();
	virtual Render3DError InitFinalRenderStates(const std::set<std::string> *oglExtensionSet);
	virtual Render3DError InitTables();
	
	virtual Render3DError LoadShaderPrograms(std::string *outVertexShaderProgram, std::string *outFragmentShaderProgram);
	virtual Render3DError SetupShaderIO();
	virtual Render3DError CreateToonTable();
	virtual Render3DError DestroyToonTable();
	virtual Render3DError UploadToonTable(const GLushort *toonTableBuffer);
	virtual Render3DError CreateClearImage();
	virtual Render3DError DestroyClearImage();
	virtual Render3DError UploadClearImage(const GLushort *clearImageColorBuffer, const GLint *clearImageDepthBuffer);
	
	virtual void GetExtensionSet(std::set<std::string> *oglExtensionSet);
	virtual Render3DError ExpandFreeTextures();
	virtual Render3DError SetupVertices(const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList, GLushort *outIndexBuffer, unsigned int *outIndexCount);
	virtual Render3DError EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const unsigned int vertIndexCount);
	virtual Render3DError DisableVertexAttributes();
	virtual Render3DError SelectRenderingFramebuffer();
	virtual Render3DError DownsampleFBO();
	virtual Render3DError ReadBackPixels();
	
	// Base rendering methods
	virtual Render3DError BeginRender(const GFX3D_State *renderState);
	virtual Render3DError PreRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList);
	virtual Render3DError DoRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList);
	virtual Render3DError PostRender();
	virtual Render3DError EndRender(const u64 frameCount);
	
	virtual Render3DError UpdateClearImage(const u16 *__restrict colorBuffer, const u16 *__restrict depthBuffer, const u8 clearStencil, const u8 xScroll, const u8 yScroll);
	virtual Render3DError UpdateToonTable(const u16 *toonTableBuffer);
	
	virtual Render3DError ClearUsingImage() const;
	virtual Render3DError ClearUsingValues(const u8 r, const u8 g, const u8 b, const u8 a, const u32 clearDepth, const u8 clearStencil) const;
	
	virtual Render3DError SetupPolygon(const POLY *thePoly);
	virtual Render3DError SetupTexture(const POLY *thePoly, bool enableTexturing);
	virtual Render3DError SetupViewport(const POLY *thePoly);
	
public:
	OpenGLRenderer_1_2();
	~OpenGLRenderer_1_2();
	
	virtual Render3DError InitExtensions();
	virtual Render3DError Reset();
	virtual Render3DError RenderFinish();
	
	virtual Render3DError DeleteTexture(const TexCacheItem *item);
};

class OpenGLRenderer_1_3 : public OpenGLRenderer_1_2
{
protected:
	virtual Render3DError CreateToonTable();
	virtual Render3DError DestroyToonTable();
	virtual Render3DError UploadToonTable(const GLushort *toonTableBuffer);
	virtual Render3DError CreateClearImage();
	virtual Render3DError DestroyClearImage();
	virtual Render3DError UploadClearImage(const GLushort *clearImageColorBuffer, const GLint *clearImageDepthBuffer);
};

class OpenGLRenderer_1_4 : public OpenGLRenderer_1_3
{
protected:
	virtual Render3DError InitFinalRenderStates(const std::set<std::string> *oglExtensionSet);
};

class OpenGLRenderer_1_5 : public OpenGLRenderer_1_4
{
protected:
	virtual Render3DError CreateVBOs();
	virtual void DestroyVBOs();
	virtual Render3DError CreatePBOs();
	virtual void DestroyPBOs();
	virtual Render3DError CreateVAOs();
	
	virtual Render3DError EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const unsigned int vertIndexCount);
	virtual Render3DError DisableVertexAttributes();
	virtual Render3DError ReadBackPixels();
		
public:
	~OpenGLRenderer_1_5();
	
	virtual Render3DError RenderFinish();
};

class OpenGLRenderer_2_0 : public OpenGLRenderer_1_5
{
protected:
	virtual Render3DError InitExtensions();
	virtual Render3DError InitFinalRenderStates(const std::set<std::string> *oglExtensionSet);
	
	virtual Render3DError SetupVertices(const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList, GLushort *outIndexBuffer, unsigned int *outIndexCount);
	virtual Render3DError EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const unsigned int vertIndexCount);
	virtual Render3DError DisableVertexAttributes();
	
	virtual Render3DError BeginRender(const GFX3D_State *renderState);
	virtual Render3DError PreRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList);
	
	virtual Render3DError SetupPolygon(const POLY *thePoly);
	virtual Render3DError SetupTexture(const POLY *thePoly, bool enableTexturing);
};

class OpenGLRenderer_2_1 : public OpenGLRenderer_2_0
{
protected:
	virtual Render3DError ReadBackPixels();
	
public:
	virtual Render3DError RenderFinish();
};

#endif
