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

#include "OGLRender.h"

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "gfx3d.h"
#include "NDSSystem.h"
#include "texcache.h"


typedef struct
{
	unsigned int major;
	unsigned int minor;
	unsigned int revision;
} OGLVersion;

static OGLVersion _OGLDriverVersion = {0, 0, 0};
static OpenGLRenderer *_OGLRenderer = NULL;
static bool isIntel965 = false;

// Lookup Tables
CACHE_ALIGN GLfloat material_8bit_to_float[256] = {0};
CACHE_ALIGN GLuint dsDepthToD24S8_LUT[32768] = {0};
const GLfloat divide5bitBy31_LUT[32]	= {0.0, 0.03225806451613, 0.06451612903226, 0.09677419354839,
									   0.1290322580645, 0.1612903225806, 0.1935483870968, 0.2258064516129,
									   0.258064516129, 0.2903225806452, 0.3225806451613, 0.3548387096774,
									   0.3870967741935, 0.4193548387097, 0.4516129032258, 0.4838709677419,
									   0.5161290322581, 0.5483870967742, 0.5806451612903, 0.6129032258065,
									   0.6451612903226, 0.6774193548387, 0.7096774193548, 0.741935483871,
									   0.7741935483871, 0.8064516129032, 0.8387096774194, 0.8709677419355,
									   0.9032258064516, 0.9354838709677, 0.9677419354839, 1.0};

static bool BEGINGL()
{
	if(oglrender_beginOpenGL) 
		return oglrender_beginOpenGL();
	else return true;
}

static void ENDGL()
{
	if(oglrender_endOpenGL) 
		oglrender_endOpenGL();
}

// Function Pointers
bool (*oglrender_init)() = NULL;
bool (*oglrender_beginOpenGL)() = NULL;
void (*oglrender_endOpenGL)() = NULL;
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
OGLEXT(PFNGLUNIFORM2FPROC, glUniform2f) // Core in v2.0
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
	INITOGLEXT(PFNGLUNIFORM2FPROC, glUniform2f) // Core in v2.0
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
static const char *vertexShader_100 = {"\
	attribute vec4 inPosition; \n\
	attribute vec2 inTexCoord0; \n\
	attribute vec3 inColor; \n\
	\n\
	uniform float polyAlpha; \n\
	uniform vec2 texScale; \n\
	\n\
	varying vec4 vtxPosition; \n\
	varying vec2 vtxTexCoord; \n\
	varying vec4 vtxColor; \n\
	\n\
	void main() \n\
	{ \n\
		mat2 texScaleMtx	= mat2(	vec2(texScale.x,        0.0), \n\
									vec2(       0.0, texScale.y)); \n\
		\n\
		vtxPosition = inPosition; \n\
		vtxTexCoord = texScaleMtx * inTexCoord0; \n\
		vtxColor = vec4(inColor * 4.0, polyAlpha); \n\
		\n\
		gl_Position = vtxPosition; \n\
	} \n\
"};

// Fragment Shader GLSL 1.00
static const char *fragmentShader_100 = {"\
	varying vec4 vtxPosition; \n\
	varying vec2 vtxTexCoord; \n\
	varying vec4 vtxColor; \n\
	\n\
	uniform sampler2D texMainRender; \n\
	uniform sampler1D texToonTable; \n\
	uniform int polyID; \n\
	uniform bool hasTexture; \n\
	uniform int polygonMode; \n\
	uniform int toonShadingMode; \n\
	uniform int oglWBuffer; \n\
	uniform bool enableAlphaTest; \n\
	uniform float alphaTestRef; \n\
	\n\
	void main() \n\
	{ \n\
		vec4 texColor = vec4(1.0, 1.0, 1.0, 1.0); \n\
		vec4 fragColor; \n\
		\n\
		if(hasTexture) \n\
		{ \n\
			texColor = texture2D(texMainRender, vtxTexCoord); \n\
		} \n\
		\n\
		fragColor = texColor; \n\
		\n\
		if(polygonMode == 0) \n\
		{ \n\
			fragColor = vtxColor * texColor; \n\
		} \n\
		else if(polygonMode == 1) \n\
		{ \n\
			if (texColor.a == 0.0 || !hasTexture) \n\
			{ \n\
				fragColor.rgb = vtxColor.rgb; \n\
			} \n\
			else if (texColor.a == 1.0) \n\
			{ \n\
				fragColor.rgb = texColor.rgb; \n\
			} \n\
			else \n\
			{ \n\
				fragColor.rgb = texColor.rgb * (1.0-texColor.a) + vtxColor.rgb * texColor.a;\n\
			} \n\
			\n\
			fragColor.a = vtxColor.a; \n\
		} \n\
		else if(polygonMode == 2) \n\
		{ \n\
			if (toonShadingMode == 0) \n\
			{ \n\
				vec3 toonColor = vec3(texture1D(texToonTable, vtxColor.r).rgb); \n\
				fragColor.rgb = texColor.rgb * toonColor.rgb;\n\
				fragColor.a = texColor.a * vtxColor.a;\n\
			} \n\
			else \n\
			{ \n\
				vec3 toonColor = vec3(texture1D(texToonTable, vtxColor.r).rgb); \n\
				fragColor.rgb = texColor.rgb * vtxColor.rgb + toonColor.rgb; \n\
				fragColor.a = texColor.a * vtxColor.a; \n\
			} \n\
		} \n\
		else if(polygonMode == 3) \n\
		{ \n\
			if (polyID != 0) \n\
			{ \n\
				fragColor = vtxColor; \n\
			} \n\
		} \n\
		\n\
		if (fragColor.a == 0.0 || (enableAlphaTest && fragColor.a < alphaTestRef)) \n\
		{ \n\
			discard; \n\
		} \n\
		\n\
		#ifdef WANT_DEPTHLOGIC \n\
		if (oglWBuffer == 1) \n\
		{ \n\
			// TODO \n\
			gl_FragDepth = (vtxPosition.z / vtxPosition.w) * 0.5 + 0.5; \n\
		} \n\
		else \n\
		{ \n\
			gl_FragDepth = (vtxPosition.z / vtxPosition.w) * 0.5 + 0.5; \n\
		} \n\
		#endif //WANT_DEPTHLOGIC \n\
		\n\
		gl_FragColor = fragColor; \n\
	} \n\
"};

FORCEINLINE u32 BGRA8888_32_To_RGBA6665_32(const u32 srcPix)
{
	const u32 dstPix = (srcPix >> 2) & 0x3F3F3F3F;
	
	return	 (dstPix & 0x0000FF00) << 16 |		// R
			 (dstPix & 0x00FF0000)       |		// G
			 (dstPix & 0xFF000000) >> 16 |		// B
			((dstPix >> 1) & 0x000000FF);		// A
}

FORCEINLINE u32 BGRA8888_32Rev_To_RGBA6665_32Rev(const u32 srcPix)
{
	const u32 dstPix = (srcPix >> 2) & 0x3F3F3F3F;
	
	return	 (dstPix & 0x00FF0000) >> 16 |		// R
			 (dstPix & 0x0000FF00)       |		// G
			 (dstPix & 0x000000FF) << 16 |		// B
			((dstPix >> 1) & 0xFF000000);		// A
}

//opengl state caching:
//This is of dubious performance assistance, but it is easy to take out so I am leaving it for now.
//every function that is xgl* can be replaced with gl* if we decide to rip this out or if anyone else
//doesnt feel like sticking with it (or if it causes trouble)

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

static void texDeleteCallback(TexCacheItem *item)
{
	_OGLRenderer->DeleteTexture(item);
}

template<bool require_profile, bool enable_3_2>
static char OGLInit(void)
{
	char result = 0;
	Render3DError error = OGLERROR_NOERR;
	
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
		INFO("OpenGL<%s,%s>: Could not initialize -- BEGINGL() failed.\n",require_profile?"force":"auto",enable_3_2?"3_2":"old");
		result = 0;
		return result;
	}
	
	// Get OpenGL info
	const char *oglVersionString = (const char *)glGetString(GL_VERSION);
	const char *oglVendorString = (const char *)glGetString(GL_VENDOR);
	const char *oglRendererString = (const char *)glGetString(GL_RENDERER);

	if(!strcmp(oglVendorString,"Intel") && strstr(oglRendererString,"965"))
		isIntel965 = true;
	
	// Check the driver's OpenGL version
	OGLGetDriverVersion(oglVersionString, &_OGLDriverVersion.major, &_OGLDriverVersion.minor, &_OGLDriverVersion.revision);
	
	if (!IsVersionSupported(OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MAJOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MINOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_REVISION))
	{
		INFO("OpenGL: Driver does not support OpenGL v%u.%u.%u or later. Disabling 3D renderer.\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
			 OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MAJOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_MINOR, OGLRENDER_MINIMUM_DRIVER_VERSION_REQUIRED_REVISION,
			 oglVersionString, oglVendorString, oglRendererString);
		
		result = 0;
		return result;
	}
	
	// Create new OpenGL rendering object
	if(enable_3_2)
	{
		if (OGLLoadEntryPoints_3_2_Func != NULL && OGLCreateRenderer_3_2_Func != NULL)
		{
			OGLLoadEntryPoints_3_2_Func();
			OGLLoadEntryPoints_Legacy(); //zero 04-feb-2013 - this seems to be necessary as well
			OGLCreateRenderer_3_2_Func(&_OGLRenderer);
		}
		else 
		{
			if(require_profile)
				return 0;
		}
	}
	
	// If the renderer doesn't initialize with OpenGL v3.2 or higher, fall back
	// to one of the lower versions.
	if (_OGLRenderer == NULL)
	{
		OGLLoadEntryPoints_Legacy();
		
		if (IsVersionSupported(2, 1, 0))
		{
			_OGLRenderer = new OpenGLRenderer_2_1;
			_OGLRenderer->SetVersion(2, 1, 0);
		}
		else if (IsVersionSupported(2, 0, 0))
		{
			_OGLRenderer = new OpenGLRenderer_2_0;
			_OGLRenderer->SetVersion(2, 0, 0);
		}
		else if (IsVersionSupported(1, 5, 0))
		{
			_OGLRenderer = new OpenGLRenderer_1_5;
			_OGLRenderer->SetVersion(1, 5, 0);
		}
		else if (IsVersionSupported(1, 4, 0))
		{
			_OGLRenderer = new OpenGLRenderer_1_4;
			_OGLRenderer->SetVersion(1, 4, 0);
		}
		else if (IsVersionSupported(1, 3, 0))
		{
			_OGLRenderer = new OpenGLRenderer_1_3;
			_OGLRenderer->SetVersion(1, 3, 0);
		}
		else if (IsVersionSupported(1, 2, 0))
		{
			_OGLRenderer = new OpenGLRenderer_1_2;
			_OGLRenderer->SetVersion(1, 2, 0);
		}
	}
	
	if (_OGLRenderer == NULL)
	{
		INFO("OpenGL: Renderer did not initialize. Disabling 3D renderer.\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
			 oglVersionString, oglVendorString, oglRendererString);
		result = 0;
		return result;
	}
	
	// Initialize OpenGL extensions
	error = _OGLRenderer->InitExtensions();
	if (error != OGLERROR_NOERR)
	{
		if ( IsVersionSupported(2, 0, 0) &&
			(error == OGLERROR_SHADER_CREATE_ERROR ||
			 error == OGLERROR_VERTEX_SHADER_PROGRAM_LOAD_ERROR ||
			 error == OGLERROR_FRAGMENT_SHADER_PROGRAM_LOAD_ERROR) )
		{
			INFO("OpenGL: Shaders are not working, even though they should be. Disabling 3D renderer.\n");
			result = 0;
			return result;
		}
		else if (IsVersionSupported(3, 0, 0) && error == OGLERROR_FBO_CREATE_ERROR && OGLLoadEntryPoints_3_2_Func != NULL)
		{
			INFO("OpenGL: FBOs are not working, even though they should be. Disabling 3D renderer.\n");
			result = 0;
			return result;
		}
	}
	
	// Initialization finished -- reset the renderer
	_OGLRenderer->Reset();
	
	ENDGL();
	
	unsigned int major = 0;
	unsigned int minor = 0;
	unsigned int revision = 0;
	_OGLRenderer->GetVersion(&major, &minor, &revision);
	
	INFO("OpenGL: Renderer initialized successfully (v%u.%u.%u).\n[ Driver Info -\n    Version: %s\n    Vendor: %s\n    Renderer: %s ]\n",
		 major, minor, revision, oglVersionString, oglVendorString, oglRendererString);
	
	return result;
}

static void OGLReset()
{
	if(!BEGINGL())
		return;
	
	_OGLRenderer->Reset();
	
	ENDGL();
}

static void OGLClose()
{
	if(!BEGINGL())
		return;
	
	delete _OGLRenderer;
	_OGLRenderer = NULL;
	
	ENDGL();
	
	Default3D_Close();
}

static void OGLRender()
{
	if(!BEGINGL())
		return;
	
	_OGLRenderer->Render(&gfx3d.renderState, gfx3d.vertlist, gfx3d.polylist, &gfx3d.indexlist, gfx3d.frameCtr);
	
	ENDGL();
}

static void OGLVramReconfigureSignal()
{
	if(!BEGINGL())
		return;
	
	_OGLRenderer->VramReconfigureSignal();
	
	ENDGL();
}

static void OGLRenderFinish()
{
	if(!BEGINGL())
		return;
	
	_OGLRenderer->RenderFinish();
	
	ENDGL();
}

//automatically select 3.2 or old profile depending on whether 3.2 is available
GPU3DInterface gpu3Dgl = {
	"OpenGL",
	OGLInit<false,true>,
	OGLReset,
	OGLClose,
	OGLRender,
	OGLRenderFinish,
	OGLVramReconfigureSignal
};

//forcibly use old profile
GPU3DInterface gpu3DglOld = {
	"OpenGL Old",
	OGLInit<true,false>,
	OGLReset,
	OGLClose,
	OGLRender,
	OGLRenderFinish,
	OGLVramReconfigureSignal
};

//forcibly use new profile
GPU3DInterface gpu3Dgl_3_2 = {
	"OpenGL 3.2",
	OGLInit<true,true>,
	OGLReset,
	OGLClose,
	OGLRender,
	OGLRenderFinish,
	OGLVramReconfigureSignal
};

OpenGLRenderer::OpenGLRenderer()
{
	versionMajor = 0;
	versionMinor = 0;
	versionRevision = 0;
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

void OpenGLRenderer::ConvertFramebuffer(const u32 *__restrict srcBuffer, u32 *dstBuffer)
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
			*dst++ = BGRA8888_32_To_RGBA6665_32(srcBuffer[i]);
#else
			*dst++ = BGRA8888_32Rev_To_RGBA6665_32Rev(srcBuffer[i]);
#endif
		}
	}
}

OpenGLRenderer_1_2::OpenGLRenderer_1_2()
{
	isVBOSupported = false;
	isPBOSupported = false;
	isFBOSupported = false;
	isMultisampledFBOSupported = false;
	isShaderSupported = false;
	isVAOSupported = false;
	
	// Init OpenGL rendering states
	ref = new OGLRenderRef;
}

OpenGLRenderer_1_2::~OpenGLRenderer_1_2()
{
	if (ref == NULL)
	{
		return;
	}
	
	glFinish();
	
	gpuScreen3DHasNewData[0] = false;
	gpuScreen3DHasNewData[1] = false;
	
	delete [] ref->color4fBuffer;
	ref->color4fBuffer = NULL;
	
	DestroyShaders();
	DestroyVAOs();
	DestroyVBOs();
	DestroyPBOs();
	DestroyFBOs();
	DestroyMultisampledFBO();
	
	//kill the tex cache to free all the texture ids
	TexCache_Reset();
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	while(!ref->freeTextureIDs.empty())
	{
		GLuint temp = ref->freeTextureIDs.front();
		ref->freeTextureIDs.pop();
		glDeleteTextures(1, &temp);
	}
	
	glFinish();
	
	// Destroy OpenGL rendering states
	delete ref;
	ref = NULL;
}

Render3DError OpenGLRenderer_1_2::InitExtensions()
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	// Get OpenGL extensions
	std::set<std::string> oglExtensionSet;
	this->GetExtensionSet(&oglExtensionSet);
	
	// Initialize OpenGL
	this->InitTables();
	
	this->isShaderSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_shader_objects") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_shader") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_fragment_shader") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_program");
	
	if (this->isShaderSupported)
	{
		std::string vertexShaderProgram;
		std::string fragmentShaderProgram;
		
		error = this->LoadShaderPrograms(&vertexShaderProgram, &fragmentShaderProgram);
		if (error == OGLERROR_NOERR)
		{
			error = this->CreateShaders(&vertexShaderProgram, &fragmentShaderProgram);
			if (error != OGLERROR_NOERR)
			{
				this->isShaderSupported = false;
				
				if (error == OGLERROR_SHADER_CREATE_ERROR)
				{
					return error;
				}
			}
			else
			{
				this->CreateToonTable();
			}
		}
		else
		{
			this->isShaderSupported = false;
		}
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
	
#if	!defined(GL_ARB_pixel_buffer_object) && !defined(GL_EXT_pixel_buffer_object)
	this->isPBOSupported = false;
#else
	this->isPBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_buffer_object") &&
							 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_pixel_buffer_object") ||
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_pixel_buffer_object"));
#endif
	if (this->isPBOSupported)
	{
		this->CreatePBOs();
	}
	
#if !defined(GL_ARB_vertex_array_object) && !defined(GL_APPLE_vertex_array_object)
	this->isVAOSupported = false;
#else
	this->isVAOSupported	= this->isShaderSupported &&
							  this->isVBOSupported &&
							 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_array_object") ||
							  this->IsExtensionPresent(&oglExtensionSet, "GL_APPLE_vertex_array_object"));
#endif
	if (this->isVAOSupported)
	{
		this->CreateVAOs();
	}
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
#if	!defined(GL_EXT_framebuffer_object)		|| \
	!defined(GL_EXT_framebuffer_blit)		|| \
	!defined(GL_EXT_packed_depth_stencil)
	
	this->isFBOSupported = false;
#else
	this->isFBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_object") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_blit") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_packed_depth_stencil");
#endif
	if (this->isFBOSupported)
	{
		error = this->CreateFBOs();
		if (error != OGLERROR_NOERR)
		{
			OGLRef.fboFinalOutputID = 0;
			this->isFBOSupported = false;
		}
	}
	else
	{
		OGLRef.fboFinalOutputID = 0;
		INFO("OpenGL: FBOs are unsupported. Some emulation features will be disabled.\n");
	}
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
#if	!defined(GL_EXT_framebuffer_object)			|| \
	!defined(GL_EXT_framebuffer_multisample)	|| \
	!defined(GL_EXT_framebuffer_blit)			|| \
	!defined(GL_EXT_packed_depth_stencil)
	
	this->isMultisampledFBOSupported = false;
#else
	this->isMultisampledFBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_object") &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_blit") &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_packed_depth_stencil") &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_multisample");
#endif
	if (this->isMultisampledFBOSupported)
	{
		error = this->CreateMultisampledFBO();
		if (error != OGLERROR_NOERR)
		{
			OGLRef.selectedRenderingFBO = 0;
			this->isMultisampledFBOSupported = false;
		}
	}
	else
	{
		OGLRef.selectedRenderingFBO = 0;
		INFO("OpenGL: Multisampled FBOs are unsupported. Multisample antialiasing will be disabled.\n");
	}
	
	this->InitTextures();
	this->InitFinalRenderStates(&oglExtensionSet); // This must be done last
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::CreateVBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenBuffersARB(1, &OGLRef.vboVertexID);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboVertexID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, VERTLIST_SIZE * sizeof(VERT), NULL, GL_STREAM_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	glGenBuffersARB(1, &OGLRef.iboIndexID);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboIndexID);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRENDER_VERT_INDEX_BUFFER_COUNT * sizeof(GLushort), NULL, GL_STREAM_DRAW_ARB);
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
	glDeleteBuffersARB(1, &OGLRef.vboVertexID);
	
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	glDeleteBuffersARB(1, &OGLRef.iboIndexID);
	
	this->isVBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreatePBOs()
{
	glGenBuffersARB(2, this->ref->pboRenderDataID);
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, this->ref->pboRenderDataID[i]);
		glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, 256 * 192 * sizeof(u32), NULL, GL_STREAM_READ_ARB);
	}
	
	glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyPBOs()
{
	if (!this->isPBOSupported)
	{
		return;
	}
	
	glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	glDeleteBuffersARB(2, this->ref->pboRenderDataID);
	
	this->isPBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::LoadShaderPrograms(std::string *outVertexShaderProgram, std::string *outFragmentShaderProgram)
{
	outVertexShaderProgram->clear();
	outFragmentShaderProgram->clear();
	
	//not only does this hardware not work, it flat-out freezes the system.
	//the problem is due to writing gl_FragDepth (it seems theres no way to successfully use it)
	//so, we disable that feature. it still works pretty well.
	if(isIntel965)
		*outFragmentShaderProgram = std::string("#define WANT_DEPTHLOGIC\n");
	
	*outVertexShaderProgram += std::string(vertexShader_100);
	*outFragmentShaderProgram += std::string(fragmentShader_100);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupShaderIO()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindAttribLocation(OGLRef.shaderProgram, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.shaderProgram, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	glBindAttribLocation(OGLRef.shaderProgram, OGLVertexAttributeID_Color, "inColor");
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::CreateShaders(const std::string *vertexShaderProgram, const std::string *fragmentShaderProgram)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	OGLRef.vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	if(!OGLRef.vertexShaderID)
	{
		INFO("OpenGL: Failed to create the vertex shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");		
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *vertexShaderProgramChar = vertexShaderProgram->c_str();
	glShaderSource(OGLRef.vertexShaderID, 1, (const GLchar **)&vertexShaderProgramChar, NULL);
	glCompileShader(OGLRef.vertexShaderID);
	if (!this->ValidateShaderCompile(OGLRef.vertexShaderID))
	{
		glDeleteShader(OGLRef.vertexShaderID);
		INFO("OpenGL: Failed to compile the vertex shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if(!OGLRef.fragmentShaderID)
	{
		glDeleteShader(OGLRef.vertexShaderID);
		INFO("OpenGL: Failed to create the fragment shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	const char *fragmentShaderProgramChar = fragmentShaderProgram->c_str();
	glShaderSource(OGLRef.fragmentShaderID, 1, (const GLchar **)&fragmentShaderProgramChar, NULL);
	glCompileShader(OGLRef.fragmentShaderID);
	if (!this->ValidateShaderCompile(OGLRef.fragmentShaderID))
	{
		glDeleteShader(OGLRef.vertexShaderID);
		glDeleteShader(OGLRef.fragmentShaderID);
		INFO("OpenGL: Failed to compile the fragment shader. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	OGLRef.shaderProgram = glCreateProgram();
	if(!OGLRef.shaderProgram)
	{
		glDeleteShader(OGLRef.vertexShaderID);
		glDeleteShader(OGLRef.fragmentShaderID);
		INFO("OpenGL: Failed to create the shader program. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glAttachShader(OGLRef.shaderProgram, OGLRef.vertexShaderID);
	glAttachShader(OGLRef.shaderProgram, OGLRef.fragmentShaderID);
	
	this->SetupShaderIO();
	
	glLinkProgram(OGLRef.shaderProgram);
	if (!this->ValidateShaderProgramLink(OGLRef.shaderProgram))
	{
		glDetachShader(OGLRef.shaderProgram, OGLRef.vertexShaderID);
		glDetachShader(OGLRef.shaderProgram, OGLRef.fragmentShaderID);
		glDeleteProgram(OGLRef.shaderProgram);
		glDeleteShader(OGLRef.vertexShaderID);
		glDeleteShader(OGLRef.fragmentShaderID);
		INFO("OpenGL: Failed to link the shader program. Disabling shaders and using fixed-function pipeline. Some emulation features will be disabled.\n");
		return OGLERROR_SHADER_CREATE_ERROR;
	}
	
	glValidateProgram(OGLRef.shaderProgram);
	glUseProgram(OGLRef.shaderProgram);
	
	// Set up shader uniforms
	GLint uniformTexSampler = glGetUniformLocation(OGLRef.shaderProgram, "texMainRender");
	glUniform1i(uniformTexSampler, 0);
	
	uniformTexSampler = glGetUniformLocation(OGLRef.shaderProgram, "texToonTable");
	glUniform1i(uniformTexSampler, OGLTextureUnitID_ToonTable);
	
	OGLRef.uniformPolyAlpha			= glGetUniformLocation(OGLRef.shaderProgram, "polyAlpha");
	OGLRef.uniformTexScale			= glGetUniformLocation(OGLRef.shaderProgram, "texScale");
	OGLRef.uniformPolyID			= glGetUniformLocation(OGLRef.shaderProgram, "polyID");
	OGLRef.uniformHasTexture		= glGetUniformLocation(OGLRef.shaderProgram, "hasTexture");
	OGLRef.uniformPolygonMode		= glGetUniformLocation(OGLRef.shaderProgram, "polygonMode");
	OGLRef.uniformToonShadingMode	= glGetUniformLocation(OGLRef.shaderProgram, "toonShadingMode");
	OGLRef.uniformWBuffer			= glGetUniformLocation(OGLRef.shaderProgram, "oglWBuffer");
	OGLRef.uniformEnableAlphaTest	= glGetUniformLocation(OGLRef.shaderProgram, "enableAlphaTest");
	OGLRef.uniformAlphaTestRef		= glGetUniformLocation(OGLRef.shaderProgram, "alphaTestRef");
	
	INFO("OpenGL: Successfully created shaders.\n");
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyShaders()
{
	if(!this->isShaderSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glUseProgram(0);
	
	glDetachShader(OGLRef.shaderProgram, OGLRef.vertexShaderID);
	glDetachShader(OGLRef.shaderProgram, OGLRef.fragmentShaderID);
	
	glDeleteProgram(OGLRef.shaderProgram);
	glDeleteShader(OGLRef.vertexShaderID);
	glDeleteShader(OGLRef.fragmentShaderID);
	
	this->DestroyToonTable();
	
	this->isShaderSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreateVAOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenVertexArrays(1, &OGLRef.vaoMainStatesID);
	glBindVertexArray(OGLRef.vaoMainStatesID);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboVertexID);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboIndexID);
	
	glEnableVertexAttribArray(OGLVertexAttributeID_Position);
	glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	glEnableVertexAttribArray(OGLVertexAttributeID_Color);
	
	glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
	glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
	glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
	
	glBindVertexArray(0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_2::DestroyVAOs()
{
	if (!this->isVAOSupported)
	{
		return;
	}
	
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &this->ref->vaoMainStatesID);
	
	this->isVAOSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreateFBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up FBO render targets
	this->CreateClearImage();
	
	// Set up FBOs
	glGenFramebuffersEXT(1, &OGLRef.fboClearImageID);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboClearImageID);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, OGLRef.texClearImageColorID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID, 0);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to created FBOs. Some emulation features will be disabled.\n");
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &OGLRef.fboClearImageID);
		this->DestroyClearImage();
		
		this->isFBOSupported = false;
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	// Set up final output FBO
	OGLRef.fboFinalOutputID = 0;
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboFinalOutputID);
	
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
	this->DestroyClearImage();
	this->isFBOSupported = false;
}

Render3DError OpenGLRenderer_1_2::CreateMultisampledFBO()
{
	// Check the maximum number of samples that the driver supports and use that.
	// Since our target resolution is only 256x192 pixels, using the most samples
	// possible is the best thing to do.
	GLint maxSamples = 0;
	glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSamples);
	
	if (maxSamples < 2)
	{
		INFO("OpenGL: Driver does not support at least 2x multisampled FBOs. Multisample antialiasing will be disabled.\n");
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	else if (maxSamples > OGLRENDER_MAX_MULTISAMPLES)
	{
		maxSamples = OGLRENDER_MAX_MULTISAMPLES;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up FBO render targets
	glGenRenderbuffersEXT(1, &OGLRef.rboMultisampleColorID);
	glGenRenderbuffersEXT(1, &OGLRef.rboMultisampleDepthStencilID);
	
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleColorID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, maxSamples, GL_RGBA, 256, 192);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleDepthStencilID);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, maxSamples, GL_DEPTH24_STENCIL8_EXT, 256, 192);
	
	// Set up multisampled rendering FBO
	glGenFramebuffersEXT(1, &OGLRef.fboMultisampleRenderID);
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboMultisampleRenderID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleColorID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleDepthStencilID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, OGLRef.rboMultisampleDepthStencilID);
	
	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		INFO("OpenGL: Failed to create multisampled FBO. Multisample antialiasing will be disabled.\n");
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &OGLRef.fboMultisampleRenderID);
		glDeleteRenderbuffersEXT(1, &OGLRef.rboMultisampleColorID);
		glDeleteRenderbuffersEXT(1, &OGLRef.rboMultisampleDepthStencilID);
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboFinalOutputID);
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
	glDeleteFramebuffersEXT(1, &OGLRef.fboMultisampleRenderID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMultisampleColorID);
	glDeleteRenderbuffersEXT(1, &OGLRef.rboMultisampleDepthStencilID);
	
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
	OGLRef.stateTexMirroredRepeat = isTexMirroredRepeatSupported ? GL_MIRRORED_REPEAT : GL_REPEAT;
	
	// Always enable depth test, and control using glDepthMask().
	glEnable(GL_DEPTH_TEST);
	
	// Map the vertex list's colors with 4 floats per color. This is being done
	// because OpenGL needs 4-colors per vertex to support translucency. (The DS
	// uses 3-colors per vertex, and adds alpha through the poly, so we can't
	// simply reference the colors+alpha from just the vertices by themselves.)
	OGLRef.color4fBuffer = this->isShaderSupported ? NULL : new GLfloat[VERTLIST_SIZE * 4];
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitTextures()
{
	this->ExpandFreeTextures();
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::InitTables()
{
	static bool needTableInit = true;
	
	if (needTableInit)
	{
		for (unsigned int i = 0; i < 256; i++)
			material_8bit_to_float[i] = (GLfloat)(i * 4) / 255.0f;
		
		for (unsigned int i = 0; i < 32768; i++)
			dsDepthToD24S8_LUT[i] = (GLuint)DS_DEPTH15TO24(i) << 8;
		
		needTableInit = false;
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::CreateToonTable()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// The toon table is a special 1D texture where each pixel corresponds
	// to a specific color in the toon table.
	glGenTextures(1, &OGLRef.texToonTableID);
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ToonTable);
	
	glBindTexture(GL_TEXTURE_1D, OGLRef.texToonTableID);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_1D, 0);
	
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DestroyToonTable()
{
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ToonTable);
	glBindTexture(GL_TEXTURE_1D, 0);
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glDeleteTextures(1, &this->ref->texToonTableID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::UploadToonTable(const GLushort *toonTableBuffer)
{
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ToonTable);
	glBindTexture(GL_TEXTURE_1D, this->ref->texToonTableID);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, toonTableBuffer);
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::CreateClearImage()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenTextures(1, &OGLRef.texClearImageColorID);
	glGenTextures(1, &OGLRef.texClearImageDepthStencilID);
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ClearImage);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, 256, 192, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
	
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DestroyClearImage()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ClearImage);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glDeleteTextures(1, &OGLRef.texClearImageColorID);
	glDeleteTextures(1, &OGLRef.texClearImageDepthStencilID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::UploadClearImage(const GLushort *clearImageColorBuffer, const GLint *clearImageDepthBuffer)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glActiveTextureARB(GL_TEXTURE0_ARB + OGLTextureUnitID_ClearImage);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageColorID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, clearImageColorBuffer);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, clearImageDepthBuffer);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
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

Render3DError OpenGLRenderer_1_2::ExpandFreeTextures()
{
	static const int kInitTextures = 128;
	GLuint oglTempTextureID[kInitTextures];
	glGenTextures(kInitTextures, oglTempTextureID);
	
	for(int i=0;i<kInitTextures;i++)
	{
		this->ref->freeTextureIDs.push(oglTempTextureID[i]);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupVertices(const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList, GLushort *outIndexBuffer, unsigned int *outIndexCount)
{
	OGLRenderRef &OGLRef = *this->ref;
	const unsigned int polyCount = polyList->count;
	unsigned int vertIndexCount = 0;
	
	for(unsigned int i = 0; i < polyCount; i++)
	{
		const POLY *poly = &polyList->list[indexList->list[i]];
		const unsigned int polyType = poly->type;
		
		if (this->isShaderSupported)
		{
			for(unsigned int j = 0; j < polyType; j++)
			{
				const GLushort vertIndex = poly->vertIndexes[j];
				
				// While we're looping through our vertices, add each vertex index to
				// a buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
				// vertices here to convert them to GL_TRIANGLES, which are much easier
				// to work with and won't be deprecated in future OpenGL versions.
				outIndexBuffer[vertIndexCount++] = vertIndex;
				if (poly->vtxFormat == GFX3D_QUADS || poly->vtxFormat == GFX3D_QUAD_STRIP)
				{
					if (j == 2)
					{
						outIndexBuffer[vertIndexCount++] = vertIndex;
					}
					else if (j == 3)
					{
						outIndexBuffer[vertIndexCount++] = poly->vertIndexes[0];
					}
				}
			}
		}
		else
		{
			const GLfloat thePolyAlpha = (!poly->isWireframe() && poly->isTranslucent()) ? divide5bitBy31_LUT[poly->getAttributeAlpha()] : 1.0f;
			
			for(unsigned int j = 0; j < polyType; j++)
			{
				const GLushort vertIndex = poly->vertIndexes[j];
				const GLushort colorIndex = vertIndex * 4;
				
				// Consolidate the vertex color and the poly alpha to our internal color buffer
				// so that OpenGL can use it.
				const VERT *vert = &vertList->list[vertIndex];
				OGLRef.color4fBuffer[colorIndex+0] = material_8bit_to_float[vert->color[0]];
				OGLRef.color4fBuffer[colorIndex+1] = material_8bit_to_float[vert->color[1]];
				OGLRef.color4fBuffer[colorIndex+2] = material_8bit_to_float[vert->color[2]];
				OGLRef.color4fBuffer[colorIndex+3] = thePolyAlpha;
				
				// While we're looping through our vertices, add each vertex index to a
				// buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
				// vertices here to convert them to GL_TRIANGLES, which are much easier
				// to work with and won't be deprecated in future OpenGL versions.
				outIndexBuffer[vertIndexCount++] = vertIndex;
				if (poly->vtxFormat == GFX3D_QUADS || poly->vtxFormat == GFX3D_QUAD_STRIP)
				{
					if (j == 2)
					{
						outIndexBuffer[vertIndexCount++] = vertIndex;
					}
					else if (j == 3)
					{
						outIndexBuffer[vertIndexCount++] = poly->vertIndexes[0];
					}
				}
			}
		}
	}
	
	*outIndexCount = vertIndexCount;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const unsigned int vertIndexCount)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(OGLRef.vaoMainStatesID);
		glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VERT) * vertList->count, vertList);
		glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
	}
	else
	{
		if (this->isShaderSupported)
		{
			glEnableVertexAttribArray(OGLVertexAttributeID_Position);
			glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
			glEnableVertexAttribArray(OGLVertexAttributeID_Color);
			
			if (this->isVBOSupported)
			{
				glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboIndexID);
				glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0, vertIndexCount * sizeof(GLushort), OGLRef.vertIndexBuffer);
				
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboVertexID);
				glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VERT) * vertList->count, vertList);
				glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
				glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
				glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
			}
			else
			{
				glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), &vertList->list[0].coord);
				glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), &vertList->list[0].texcoord);
				glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), &vertList->list[0].color);
			}
		}
		else
		{
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glEnableClientState(GL_VERTEX_ARRAY);
			
			if (this->isVBOSupported)
			{
				glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, OGLRef.iboIndexID);
				glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0, vertIndexCount * sizeof(GLushort), OGLRef.vertIndexBuffer);
				
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
				glColorPointer(4, GL_FLOAT, 0, OGLRef.color4fBuffer);
				
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, OGLRef.vboVertexID);
				glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VERT) * vertList->count, vertList);
				glVertexPointer(4, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
				glTexCoordPointer(2, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
			}
			else
			{
				glVertexPointer(4, GL_FLOAT, sizeof(VERT), &vertList->list[0].coord);
				glTexCoordPointer(2, GL_FLOAT, sizeof(VERT), &vertList->list[0].texcoord);
				glColorPointer(4, GL_FLOAT, 0, OGLRef.color4fBuffer);
			}
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
		
		if (this->isVBOSupported)
		{
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SelectRenderingFramebuffer()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isMultisampledFBOSupported)
	{
		OGLRef.selectedRenderingFBO = CommonSettings.GFX3D_Renderer_Multisample ? OGLRef.fboMultisampleRenderID : OGLRef.fboFinalOutputID;
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DownsampleFBO()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->isMultisampledFBOSupported || OGLRef.selectedRenderingFBO != OGLRef.fboMultisampleRenderID)
	{
		return OGLERROR_NOERR;
	}
	
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.fboFinalOutputID);
	glBlitFramebufferEXT(0, 0, 256, 192, 0, 0, 256, 192, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.fboFinalOutputID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ReadBackPixels()
{
	const unsigned int i = this->doubleBufferIndex;
	
	if (this->isPBOSupported)
	{
		this->DownsampleFBO();
		
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, this->ref->pboRenderDataID[i]);
		glReadPixels(0, 0, 256, 192, GL_BGRA, GL_UNSIGNED_BYTE, 0);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	
	this->gpuScreen3DHasNewData[i] = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DeleteTexture(const TexCacheItem *item)
{
	this->ref->freeTextureIDs.push((GLuint)item->texid);
	if(this->currTexture == item)
	{
		this->currTexture = NULL;
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::BeginRender(const GFX3D_State *renderState)
{
	OGLRenderRef &OGLRef = *this->ref;
	this->doubleBufferIndex = (this->doubleBufferIndex + 1) & 0x01;
	
	this->SelectRenderingFramebuffer();
	
	if (this->isShaderSupported)
	{
		glUniform1i(OGLRef.uniformEnableAlphaTest, renderState->enableAlphaTest ? GL_TRUE : GL_FALSE);
		glUniform1f(OGLRef.uniformAlphaTestRef, divide5bitBy31_LUT[renderState->alphaTestRef]);
		glUniform1i(OGLRef.uniformToonShadingMode, renderState->shading);
		glUniform1i(OGLRef.uniformWBuffer, renderState->wbuffer);
	}
	else
	{
		if(renderState->enableAlphaTest && (renderState->alphaTestRef > 0))
		{
			glAlphaFunc(GL_GEQUAL, divide5bitBy31_LUT[renderState->alphaTestRef]);
		}
		else
		{
			glAlphaFunc(GL_GREATER, 0);
		}
	}
	
	if(renderState->enableAlphaBlending)
	{
		glEnable(GL_BLEND);
	}
	else
	{
		glDisable(GL_BLEND);
	}
	
	glDepthMask(GL_TRUE);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::PreRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList)
{
	OGLRenderRef &OGLRef = *this->ref;
	unsigned int vertIndexCount = 0;
	
	if (!this->isShaderSupported)
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
	}
	
	this->SetupVertices(vertList, polyList, indexList, OGLRef.vertIndexBuffer, &vertIndexCount);
	this->EnableVertexAttributes(vertList, OGLRef.vertIndexBuffer, vertIndexCount);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::DoRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList)
{
	OGLRenderRef &OGLRef = *this->ref;
	u32 lastTexParams = 0;
	u32 lastTexPalette = 0;
	u32 lastPolyAttr = 0;
	u32 lastViewport = 0xFFFFFFFF;
	const unsigned int polyCount = polyList->count;
	GLushort *indexBufferPtr = this->isVBOSupported ? 0 : OGLRef.vertIndexBuffer;
	
	// Map GFX3D_QUADS and GFX3D_QUAD_STRIP to GL_TRIANGLES since we will convert them.
	//
	// Also map GFX3D_TRIANGLE_STRIP to GL_TRIANGLES. This is okay since this is actually
	// how the POLY struct stores triangle strip vertices, which is in sets of 3 vertices
	// each. This redefinition is necessary since uploading more than 3 indices at a time
	// will cause glDrawElements() to draw the triangle strip incorrectly.
	static const GLenum oglPrimitiveType[]	= {GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES,
											   GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_STRIP, GL_LINE_STRIP};
	
	static const unsigned int indexIncrementLUT[] = {3, 6, 3, 6, 3, 4, 3, 4};
	
	for(unsigned int i = 0; i < polyCount; i++)
	{
		const POLY *poly = &polyList->list[indexList->list[i]];
		
		// Set up the polygon if it changed
		if(lastPolyAttr != poly->polyAttr || i == 0)
		{
			lastPolyAttr = poly->polyAttr;
			this->SetupPolygon(poly);
		}
		
		// Set up the texture if it changed
		if(lastTexParams != poly->texParam || lastTexPalette != poly->texPalette || i == 0)
		{
			lastTexParams = poly->texParam;
			lastTexPalette = poly->texPalette;
			this->SetupTexture(poly, renderState->enableTexturing);
		}
		
		// Set up the viewport if it changed
		if(lastViewport != poly->viewport || i == 0)
		{
			lastViewport = poly->viewport;
			this->SetupViewport(poly);
		}
		
		// In wireframe mode, redefine all primitives as GL_LINE_LOOP rather than
		// setting the polygon mode to GL_LINE though glPolygonMode(). Not only is
		// drawing more accurate this way, but it also allows GFX3D_QUADS and
		// GFX3D_QUAD_STRIP primitives to properly draw as wireframe without the
		// extra diagonal line.
		const GLenum polyPrimitive = !poly->isWireframe() ? oglPrimitiveType[poly->vtxFormat] : GL_LINE_LOOP;
		
		// Render the polygon
		const unsigned int vertIndexCount = indexIncrementLUT[poly->vtxFormat];
		glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, indexBufferPtr);
		indexBufferPtr += vertIndexCount;
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::PostRender()
{
	this->DisableVertexAttributes();
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::EndRender(const u64 frameCount)
{
	//needs to happen before endgl because it could free some textureids for expired cache items
	TexCache_EvictFrame();
	
	this->ReadBackPixels();
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::UpdateClearImage(const u16 *__restrict colorBuffer, const u16 *__restrict depthBuffer, const u8 clearStencil, const u8 xScroll, const u8 yScroll)
{
	static const size_t pixelsPerLine = 256;
	static const size_t lineCount = 192;
	static const size_t totalPixelCount = pixelsPerLine * lineCount;
	static const size_t bufferSize = totalPixelCount * sizeof(u16);
	
	static CACHE_ALIGN GLushort clearImageColorBuffer[totalPixelCount] = {0};
	static CACHE_ALIGN GLint clearImageDepthBuffer[totalPixelCount] = {0};
	static CACHE_ALIGN u16 lastColorBuffer[totalPixelCount] = {0};
	static CACHE_ALIGN u16 lastDepthBuffer[totalPixelCount] = {0};
	static u8 lastXScroll = 0;
	static u8 lastYScroll = 0;
	
	if (!this->isFBOSupported)
	{
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	
	if (lastXScroll != xScroll ||
		lastYScroll != yScroll ||
		memcmp(colorBuffer, lastColorBuffer, bufferSize) ||
		memcmp(depthBuffer, lastDepthBuffer, bufferSize) )
	{
		lastXScroll = xScroll;
		lastYScroll = yScroll;
		memcpy(lastColorBuffer, colorBuffer, bufferSize);
		memcpy(lastDepthBuffer, depthBuffer, bufferSize);
		
		unsigned int dd = totalPixelCount - pixelsPerLine;
		
		for(unsigned int iy = 0; iy < lineCount; iy++)
		{
			const unsigned int y = ((iy + yScroll) & 0xFF) << 8;
			
			for(unsigned int ix = 0; ix < pixelsPerLine; ix++)
			{
				const unsigned int x = (ix + xScroll) & 0xFF;
				const unsigned int adr = y + x;
				
				clearImageColorBuffer[dd] = colorBuffer[adr];
				clearImageDepthBuffer[dd] = dsDepthToD24S8_LUT[depthBuffer[adr] & 0x7FFF] | clearStencil;
				
				dd++;
			}
			
			dd -= pixelsPerLine * 2;
		}
		
		this->UploadClearImage(clearImageColorBuffer, clearImageDepthBuffer);
	}
	
	this->clearImageStencilValue = clearStencil;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::UpdateToonTable(const u16 *toonTableBuffer)
{
	// Update the toon table if it changed.
	if (memcmp(this->currentToonTable16, toonTableBuffer, sizeof(this->currentToonTable16)))
	{
		memcpy(this->currentToonTable16, toonTableBuffer, sizeof(this->currentToonTable16));
		this->toonTableNeedsUpdate = true;
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ClearUsingImage() const
{
	static u8 lastClearStencil = 0;
	
	if (!this->isFBOSupported)
	{
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, OGLRef.fboClearImageID);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
	glBlitFramebufferEXT(0, 0, 256, 192, 0, 0, 256, 192, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OGLRef.selectedRenderingFBO);
	
	// It might seem wasteful to be doing a separate glClear(GL_STENCIL_BUFFER_BIT) instead
	// of simply blitting the stencil buffer with everything else.
	//
	// We do this because glBlitFramebufferEXT() for GL_STENCIL_BUFFER_BIT has been tested
	// to be unsupported on ATI/AMD GPUs running in compatibility mode. So we do the separate
	// glClear() for GL_STENCIL_BUFFER_BIT to keep these GPUs working.
	if (lastClearStencil == this->clearImageStencilValue)
	{
		lastClearStencil = this->clearImageStencilValue;
		glClearStencil(lastClearStencil);
	}
	
	glClear(GL_STENCIL_BUFFER_BIT);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::ClearUsingValues(const u8 r, const u8 g, const u8 b, const u8 a, const u32 clearDepth, const u8 clearStencil) const
{
	static u8 last_r = 0;
	static u8 last_g = 0;
	static u8 last_b = 0;
	static u8 last_a = 0;
	static u32 lastClearDepth = 0;
	static u8 lastClearStencil = 0;
	
	if (r != last_r || g != last_g || b != last_b || a != last_a)
	{
		last_r = r;
		last_g = g;
		last_b = b;
		last_a = a;
		glClearColor(divide5bitBy31_LUT[r], divide5bitBy31_LUT[g], divide5bitBy31_LUT[b], divide5bitBy31_LUT[a]);
	}
	
	if (clearDepth != lastClearDepth)
	{
		lastClearDepth = clearDepth;
		glClearDepth((GLfloat)clearDepth / (GLfloat)0x00FFFFFF);
	}
	
	if (clearStencil != lastClearStencil)
	{
		lastClearStencil = clearStencil;
		glClearStencil(clearStencil);
	}
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupPolygon(const POLY *thePoly)
{
	static unsigned int lastTexBlendMode = 0;
	static int lastStencilState = -1;
	
	OGLRenderRef &OGLRef = *this->ref;
	const PolygonAttributes attr = thePoly->getAttributes();
	
	// Set up polygon ID
	if (this->isShaderSupported)
	{
		glUniform1i(OGLRef.uniformPolyID, attr.polygonID);
	}
	
	// Set up alpha value
	if (this->isShaderSupported)
	{
		const GLfloat thePolyAlpha = (!attr.isWireframe && attr.isTranslucent) ? divide5bitBy31_LUT[attr.alpha] : 1.0f;
		glUniform1f(OGLRef.uniformPolyAlpha, thePolyAlpha);
	}
	
	// Set up depth test mode
	static const GLenum oglDepthFunc[2] = {GL_LESS, GL_EQUAL};
	glDepthFunc(oglDepthFunc[attr.enableDepthTest]);
	
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
				glStencilFunc(GL_ALWAYS, 65, 255);
				glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
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
				glStencilFunc(GL_EQUAL, 65, 255);
				glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
		}
	}
	else
	{
		xglEnable(GL_STENCIL_TEST);
		if(attr.isTranslucent)
		{
			lastStencilState = 3;
			glStencilFunc(GL_NOTEQUAL, attr.polygonID, 255);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		else if(lastStencilState != 2)
		{
			lastStencilState = 2;
			glStencilFunc(GL_ALWAYS, 64, 255);
			glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
	}
	
	if(attr.isTranslucent && !attr.enableAlphaDepthWrite)
	{
		enableDepthWrite = GL_FALSE;
	}
	
	glDepthMask(enableDepthWrite);
	
	// Set up texture blending mode
	if(attr.polygonMode != lastTexBlendMode)
	{
		lastTexBlendMode = attr.polygonMode;
		
		if(this->isShaderSupported)
		{
			glUniform1i(OGLRef.uniformPolygonMode, attr.polygonMode);
			
			// Update the toon table if necessary
			if (this->toonTableNeedsUpdate && attr.polygonMode == 2)
			{
				this->UploadToonTable(this->currentToonTable16);
				this->toonTableNeedsUpdate = false;
			}
		}
		else
		{
			static const GLint oglTexBlendMode[4] = {GL_MODULATE, GL_DECAL, GL_MODULATE, GL_MODULATE};
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, oglTexBlendMode[attr.polygonMode]);
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupTexture(const POLY *thePoly, bool enableTexturing)
{
	OGLRenderRef &OGLRef = *this->ref;
	const PolygonTexParams params = thePoly->getTexParams();
	
	// Check if we need to use textures
	if (thePoly->texParam == 0 || params.texFormat == TEXMODE_NONE || !enableTexturing)
	{
		if (this->isShaderSupported)
		{
			glUniform1i(OGLRef.uniformHasTexture, GL_FALSE);
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
		glUniform1i(OGLRef.uniformHasTexture, GL_TRUE);
	}
	else
	{
		glEnable(GL_TEXTURE_2D);
	}
	
	//	texCacheUnit.TexCache_SetTexture<TexFormat_32bpp>(format, texpal);
	TexCacheItem *newTexture = TexCache_SetTexture(TexFormat_32bpp, thePoly->texParam, thePoly->texPalette);
	if(newTexture != this->currTexture)
	{
		this->currTexture = newTexture;
		//has the ogl renderer initialized the texture?
		if(!this->currTexture->deleteCallback)
		{
			this->currTexture->deleteCallback = texDeleteCallback;
			
			if(OGLRef.freeTextureIDs.empty())
			{
				this->ExpandFreeTextures();
			}
			
			this->currTexture->texid = (u64)OGLRef.freeTextureIDs.front();
			OGLRef.freeTextureIDs.pop();
			
			glBindTexture(GL_TEXTURE_2D, (GLuint)this->currTexture->texid);
			
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (params.enableRepeatS ? (params.enableMirroredRepeatS ? OGLRef.stateTexMirroredRepeat : GL_REPEAT) : GL_CLAMP_TO_EDGE));
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (params.enableRepeatT ? (params.enableMirroredRepeatT ? OGLRef.stateTexMirroredRepeat : GL_REPEAT) : GL_CLAMP_TO_EDGE));
			
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
						 this->currTexture->sizeX, this->currTexture->sizeY, 0,
						 GL_RGBA, GL_UNSIGNED_BYTE, this->currTexture->decoded);
		}
		else
		{
			//otherwise, just bind it
			glBindTexture(GL_TEXTURE_2D, (GLuint)this->currTexture->texid);
		}
		
		if (this->isShaderSupported)
		{
			glUniform2f(OGLRef.uniformTexScale, this->currTexture->invSizeX, this->currTexture->invSizeY);
		}
		else
		{
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();
			glScalef(this->currTexture->invSizeX, this->currTexture->invSizeY, 1.0f);
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::SetupViewport(const POLY *thePoly)
{
	VIEWPORT viewport;
	viewport.decode(thePoly->viewport);
	glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::Reset()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	this->gpuScreen3DHasNewData[0] = false;
	this->gpuScreen3DHasNewData[1] = false;
	
	glFinish();
	
	for (unsigned int i = 0; i < 2; i++)
	{
		memset(this->GPU_screen3D[i], 0, sizeof(this->GPU_screen3D[i]));
	}
	
	memset(currentToonTable16, 0, sizeof(currentToonTable16));
	this->toonTableNeedsUpdate = true;
	
	if(this->isShaderSupported)
	{
		glUniform1f(OGLRef.uniformPolyAlpha, 1.0f);
		glUniform2f(OGLRef.uniformTexScale, 1.0f, 1.0f);
		glUniform1i(OGLRef.uniformPolyID, 0);
		glUniform1i(OGLRef.uniformHasTexture, GL_FALSE);
		glUniform1i(OGLRef.uniformPolygonMode, 0);
		glUniform1i(OGLRef.uniformToonShadingMode, 0);
		glUniform1i(OGLRef.uniformWBuffer, 0);
		glUniform1i(OGLRef.uniformEnableAlphaTest, GL_TRUE);
		glUniform1f(OGLRef.uniformAlphaTestRef, 0.0f);
	}
	else
	{
		glEnable(GL_NORMALIZE);
		glEnable(GL_TEXTURE_1D);
		glEnable(GL_TEXTURE_2D);
		glAlphaFunc(GL_GREATER, 0);
		glEnable(GL_ALPHA_TEST);
		
		memset(OGLRef.color4fBuffer, 0, VERTLIST_SIZE * 4 * sizeof(GLfloat));
	}
	
	memset(OGLRef.vertIndexBuffer, 0, OGLRENDER_VERT_INDEX_BUFFER_COUNT * sizeof(GLushort));
	this->currTexture = NULL;
	this->doubleBufferIndex = 0;
	this->clearImageStencilValue = 0;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_2::RenderFinish()
{
	const unsigned int i = this->doubleBufferIndex;
	
	if (!this->gpuScreen3DHasNewData[i])
	{
		return OGLERROR_NOERR;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isPBOSupported)
	{
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, OGLRef.pboRenderDataID[i]);
		
		const u32 *__restrict mappedBufferPtr = (u32 *__restrict)glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
		if (mappedBufferPtr != NULL)
		{
			this->ConvertFramebuffer(mappedBufferPtr, (u32 *)gfx3d_convertedScreen);
			glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
		}
		
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	else
	{
		this->DownsampleFBO();
		
		u32 *__restrict workingBuffer = this->GPU_screen3D[i];
		glReadPixels(0, 0, 256, 192, GL_BGRA, GL_UNSIGNED_BYTE, workingBuffer);
		this->ConvertFramebuffer(workingBuffer, (u32 *)gfx3d_convertedScreen);
	}
	
	this->gpuScreen3DHasNewData[i] = false;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_3::CreateToonTable()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// The toon table is a special 1D texture where each pixel corresponds
	// to a specific color in the toon table.
	glGenTextures(1, &OGLRef.texToonTableID);
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ToonTable);
	
	glBindTexture(GL_TEXTURE_1D, OGLRef.texToonTableID);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_1D, 0);
	
	glActiveTexture(GL_TEXTURE0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_3::DestroyToonTable()
{
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ToonTable);
	glBindTexture(GL_TEXTURE_1D, 0);
	glActiveTexture(GL_TEXTURE0);
	glDeleteTextures(1, &this->ref->texToonTableID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_3::UploadToonTable(const GLushort *toonTableBuffer)
{
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ToonTable);
	glBindTexture(GL_TEXTURE_1D, this->ref->texToonTableID);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, toonTableBuffer);
	glActiveTexture(GL_TEXTURE0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_3::CreateClearImage()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenTextures(1, &OGLRef.texClearImageColorID);
	glGenTextures(1, &OGLRef.texClearImageDepthStencilID);
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ClearImage);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8_EXT, 256, 192, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
	
	glActiveTexture(GL_TEXTURE0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_3::DestroyClearImage()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ClearImage);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glDeleteTextures(1, &OGLRef.texClearImageColorID);
	glDeleteTextures(1, &OGLRef.texClearImageDepthStencilID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_3::UploadClearImage(const GLushort *clearImageColorBuffer, const GLint *clearImageDepthBuffer)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glActiveTexture(GL_TEXTURE0 + OGLTextureUnitID_ClearImage);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageColorID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, clearImageColorBuffer);
	glBindTexture(GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, clearImageDepthBuffer);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	glActiveTexture(GL_TEXTURE0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_4::InitFinalRenderStates(const std::set<std::string> *oglExtensionSet)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	bool isBlendEquationSeparateSupported = this->IsExtensionPresent(oglExtensionSet, "GL_EXT_blend_equation_separate");
	
	// Blending Support
	if (isBlendEquationSeparateSupported)
	{
		// we want to use alpha destination blending so we can track the last-rendered alpha value
		// test: new super mario brothers renders the stormclouds at the beginning
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_DST_ALPHA);
		glBlendEquationSeparateEXT(GL_FUNC_ADD, GL_MAX);
	}
	else
	{
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_DST_ALPHA);
	}
	
	// Mirrored Repeat Mode Support
	OGLRef.stateTexMirroredRepeat = GL_MIRRORED_REPEAT;
	
	// Always enable depth test, and control using glDepthMask().
	glEnable(GL_DEPTH_TEST);
	
	// Map the vertex list's colors with 4 floats per color. This is being done
	// because OpenGL needs 4-colors per vertex to support translucency. (The DS
	// uses 3-colors per vertex, and adds alpha through the poly, so we can't
	// simply reference the colors+alpha from just the vertices by themselves.)
	OGLRef.color4fBuffer = this->isShaderSupported ? NULL : new GLfloat[VERTLIST_SIZE * 4];
	
	return OGLERROR_NOERR;
}

OpenGLRenderer_1_5::~OpenGLRenderer_1_5()
{
	glFinish();
	
	DestroyVAOs();
	DestroyVBOs();
	DestroyPBOs();
}

Render3DError OpenGLRenderer_1_5::CreateVBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenBuffers(1, &OGLRef.vboVertexID);
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboVertexID);
	glBufferData(GL_ARRAY_BUFFER, VERTLIST_SIZE * sizeof(VERT), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glGenBuffers(1, &OGLRef.iboIndexID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboIndexID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, OGLRENDER_VERT_INDEX_BUFFER_COUNT * sizeof(GLushort), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_5::DestroyVBOs()
{
	if (!this->isVBOSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &OGLRef.vboVertexID);
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &OGLRef.iboIndexID);
	
	this->isVBOSupported = false;
}

Render3DError OpenGLRenderer_1_5::CreatePBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenBuffers(2, OGLRef.pboRenderDataID);
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, OGLRef.pboRenderDataID[i]);
		glBufferData(GL_PIXEL_PACK_BUFFER_ARB, 256 * 192 * sizeof(u32), NULL, GL_STREAM_READ);
	}
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_1_5::DestroyPBOs()
{
	if (!this->isPBOSupported)
	{
		return;
	}
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
	glDeleteBuffers(2, this->ref->pboRenderDataID);
	
	this->isPBOSupported = false;
}

Render3DError OpenGLRenderer_1_5::CreateVAOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenVertexArrays(1, &OGLRef.vaoMainStatesID);
	glBindVertexArray(OGLRef.vaoMainStatesID);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboVertexID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboIndexID);
	
	glEnableVertexAttribArray(OGLVertexAttributeID_Position);
	glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	glEnableVertexAttribArray(OGLVertexAttributeID_Color);
	
	glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
	glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
	glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
	
	glBindVertexArray(0);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_5::EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const unsigned int vertIndexCount)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(OGLRef.vaoMainStatesID);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
	}
	else
	{
		if (this->isShaderSupported)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboIndexID);
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
			
			glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboVertexID);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
			
			glEnableVertexAttribArray(OGLVertexAttributeID_Position);
			glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
			glEnableVertexAttribArray(OGLVertexAttributeID_Color);
			
			glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
			glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
			glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
		}
		else
		{
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glEnableClientState(GL_VERTEX_ARRAY);
			
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboIndexID);
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
			
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glColorPointer(4, GL_FLOAT, 0, OGLRef.color4fBuffer);
			
			glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboVertexID);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
			glVertexPointer(4, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
			glTexCoordPointer(2, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_5::DisableVertexAttributes()
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
		
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_5::ReadBackPixels()
{
	const unsigned int i = this->doubleBufferIndex;
	
	if (this->isPBOSupported)
	{
		this->DownsampleFBO();
		
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, this->ref->pboRenderDataID[i]);
		glReadPixels(0, 0, 256, 192, GL_BGRA, GL_UNSIGNED_BYTE, 0);
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	
	this->gpuScreen3DHasNewData[i] = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_1_5::RenderFinish()
{
	const unsigned int i = this->doubleBufferIndex;
	
	if (!this->gpuScreen3DHasNewData[i])
	{
		return OGLERROR_NOERR;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isPBOSupported)
	{
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, OGLRef.pboRenderDataID[i]);
		
		const u32 *__restrict mappedBufferPtr = (u32 *__restrict)glMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
		if (mappedBufferPtr != NULL)
		{
			this->ConvertFramebuffer(mappedBufferPtr, (u32 *)gfx3d_convertedScreen);
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB);
		}
		
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	else
	{
		this->DownsampleFBO();
		
		u32 *__restrict workingBuffer = this->GPU_screen3D[i];
		glReadPixels(0, 0, 256, 192, GL_BGRA, GL_UNSIGNED_BYTE, workingBuffer);
		this->ConvertFramebuffer(workingBuffer, (u32 *)gfx3d_convertedScreen);
	}
	
	this->gpuScreen3DHasNewData[i] = false;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::InitExtensions()
{
	Render3DError error = OGLERROR_NOERR;
	OGLRenderRef &OGLRef = *this->ref;
	
	// Get OpenGL extensions
	std::set<std::string> oglExtensionSet;
	this->GetExtensionSet(&oglExtensionSet);
	
	// Initialize OpenGL
	this->InitTables();
	
	// Load and create shaders. Return on any error, since a v2.0 driver will assume that shaders are available.
	this->isShaderSupported	= true;
	
	std::string vertexShaderProgram;
	std::string fragmentShaderProgram;
	error = this->LoadShaderPrograms(&vertexShaderProgram, &fragmentShaderProgram);
	if (error != OGLERROR_NOERR)
	{
		this->isShaderSupported = false;
		return error;
	}
	
	error = this->CreateShaders(&vertexShaderProgram, &fragmentShaderProgram);
	if (error != OGLERROR_NOERR)
	{
		this->isShaderSupported = false;
		return error;
	}
	
	this->CreateToonTable();
	
	this->isVBOSupported = true;
	this->CreateVBOs();
	
#if	!defined(GL_ARB_pixel_buffer_object) && !defined(GL_EXT_pixel_buffer_object)
	this->isPBOSupported = false;
#else
	this->isPBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_buffer_object") &&
							 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_pixel_buffer_object") ||
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_pixel_buffer_object"));
#endif
	if (this->isPBOSupported)
	{
		this->CreatePBOs();
	}
	
#if !defined(GL_ARB_vertex_array_object) && !defined(GL_APPLE_vertex_array_object)
	this->isVAOSupported = false;
#else
	this->isVAOSupported	= this->isShaderSupported &&
							  this->isVBOSupported &&
							 (this->IsExtensionPresent(&oglExtensionSet, "GL_ARB_vertex_array_object") ||
							  this->IsExtensionPresent(&oglExtensionSet, "GL_APPLE_vertex_array_object"));
#endif
	if (this->isVAOSupported)
	{
		this->CreateVAOs();
	}
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
#if	!defined(GL_EXT_framebuffer_object)		|| \
	!defined(GL_EXT_framebuffer_blit)		|| \
	!defined(GL_EXT_packed_depth_stencil)
	
	this->isFBOSupported = false;
#else
	this->isFBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_object") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_blit") &&
							  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_packed_depth_stencil");
#endif
	if (this->isFBOSupported)
	{
		error = this->CreateFBOs();
		if (error != OGLERROR_NOERR)
		{
			OGLRef.fboFinalOutputID = 0;
			this->isFBOSupported = false;
		}
	}
	else
	{
		OGLRef.fboFinalOutputID = 0;
		INFO("OpenGL: FBOs are unsupported. Some emulation features will be disabled.\n");
	}
	
	// Don't use ARB versions since we're using the EXT versions for backwards compatibility.
#if	!defined(GL_EXT_framebuffer_object)			|| \
	!defined(GL_EXT_framebuffer_multisample)	|| \
	!defined(GL_EXT_framebuffer_blit)			|| \
	!defined(GL_EXT_packed_depth_stencil)
	
	this->isMultisampledFBOSupported = false;
#else
	this->isMultisampledFBOSupported	= this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_object") &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_blit") &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_packed_depth_stencil") &&
										  this->IsExtensionPresent(&oglExtensionSet, "GL_EXT_framebuffer_multisample");
#endif
	if (this->isMultisampledFBOSupported)
	{
		error = this->CreateMultisampledFBO();
		if (error != OGLERROR_NOERR)
		{
			OGLRef.selectedRenderingFBO = 0;
			this->isMultisampledFBOSupported = false;
		}
	}
	else
	{
		OGLRef.selectedRenderingFBO = 0;
		INFO("OpenGL: Multisampled FBOs are unsupported. Multisample antialiasing will be disabled.\n");
	}
	
	this->InitTextures();
	this->InitFinalRenderStates(&oglExtensionSet); // This must be done last
	
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
	
	// Always enable depth test, and control using glDepthMask().
	glEnable(GL_DEPTH_TEST);
	
	// Ignore our color buffer since we'll transfer the polygon alpha through a uniform.
	OGLRef.color4fBuffer = NULL;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::SetupVertices(const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList, GLushort *outIndexBuffer, unsigned int *outIndexCount)
{
	const unsigned int polyCount = polyList->count;
	unsigned int vertIndexCount = 0;
	
	for(unsigned int i = 0; i < polyCount; i++)
	{
		const POLY *poly = &polyList->list[indexList->list[i]];
		const unsigned int polyType = poly->type;
		
		for(unsigned int j = 0; j < polyType; j++)
		{
			const GLushort vertIndex = poly->vertIndexes[j];
			
			// While we're looping through our vertices, add each vertex index to
			// a buffer. For GFX3D_QUADS and GFX3D_QUAD_STRIP, we also add additional
			// vertices here to convert them to GL_TRIANGLES, which are much easier
			// to work with and won't be deprecated in future OpenGL versions.
			outIndexBuffer[vertIndexCount++] = vertIndex;
			if (poly->vtxFormat == GFX3D_QUADS || poly->vtxFormat == GFX3D_QUAD_STRIP)
			{
				if (j == 2)
				{
					outIndexBuffer[vertIndexCount++] = vertIndex;
				}
				else if (j == 3)
				{
					outIndexBuffer[vertIndexCount++] = poly->vertIndexes[0];
				}
			}
		}
	}
	
	*outIndexCount = vertIndexCount;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const unsigned int vertIndexCount)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (this->isVAOSupported)
	{
		glBindVertexArray(OGLRef.vaoMainStatesID);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
	}
	else
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboIndexID);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
		
		glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboVertexID);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
		
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
		glEnableVertexAttribArray(OGLVertexAttributeID_Color);
		
		glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
		glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
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
		
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::BeginRender(const GFX3D_State *renderState)
{
	OGLRenderRef &OGLRef = *this->ref;
	this->doubleBufferIndex = (this->doubleBufferIndex + 1) & 0x01;
	
	this->SelectRenderingFramebuffer();
	
	glUniform1i(OGLRef.uniformEnableAlphaTest, renderState->enableAlphaTest ? GL_TRUE : GL_FALSE);
	glUniform1f(OGLRef.uniformAlphaTestRef, divide5bitBy31_LUT[renderState->alphaTestRef]);
	glUniform1i(OGLRef.uniformToonShadingMode, renderState->shading);
	glUniform1i(OGLRef.uniformWBuffer, renderState->wbuffer);
	
	if(renderState->enableAlphaBlending)
	{
		glEnable(GL_BLEND);
	}
	else
	{
		glDisable(GL_BLEND);
	}
	
	glDepthMask(GL_TRUE);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::PreRender(const GFX3D_State *renderState, const VERTLIST *vertList, const POLYLIST *polyList, const INDEXLIST *indexList)
{
	OGLRenderRef &OGLRef = *this->ref;
	unsigned int vertIndexCount = 0;
	
	this->SetupVertices(vertList, polyList, indexList, OGLRef.vertIndexBuffer, &vertIndexCount);
	this->EnableVertexAttributes(vertList, OGLRef.vertIndexBuffer, vertIndexCount);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::SetupPolygon(const POLY *thePoly)
{
	static unsigned int lastTexBlendMode = 0;
	static int lastStencilState = -1;
	
	OGLRenderRef &OGLRef = *this->ref;
	const PolygonAttributes attr = thePoly->getAttributes();
	
	// Set up polygon ID
	glUniform1i(OGLRef.uniformPolyID, attr.polygonID);
	
	// Set up alpha value
	const GLfloat thePolyAlpha = (!attr.isWireframe && attr.isTranslucent) ? divide5bitBy31_LUT[attr.alpha] : 1.0f;
	glUniform1f(OGLRef.uniformPolyAlpha, thePolyAlpha);
	
	// Set up depth test mode
	static const GLenum oglDepthFunc[2] = {GL_LESS, GL_EQUAL};
	glDepthFunc(oglDepthFunc[attr.enableDepthTest]);
	
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
				glStencilFunc(GL_ALWAYS, 65, 255);
				glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
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
				glStencilFunc(GL_EQUAL, 65, 255);
				glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
		}
	}
	else
	{
		xglEnable(GL_STENCIL_TEST);
		if(attr.isTranslucent)
		{
			lastStencilState = 3;
			glStencilFunc(GL_NOTEQUAL, attr.polygonID, 255);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		else if(lastStencilState != 2)
		{
			lastStencilState = 2;
			glStencilFunc(GL_ALWAYS, 64, 255);
			glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
	}
	
	if(attr.isTranslucent && !attr.enableAlphaDepthWrite)
	{
		enableDepthWrite = GL_FALSE;
	}
	
	glDepthMask(enableDepthWrite);
	
	// Set up texture blending mode
	if(attr.polygonMode != lastTexBlendMode)
	{
		lastTexBlendMode = attr.polygonMode;
		glUniform1i(OGLRef.uniformPolygonMode, attr.polygonMode);
		
		// Update the toon table if necessary
		if (this->toonTableNeedsUpdate && attr.polygonMode == 2)
		{
			this->UploadToonTable(this->currentToonTable16);
			this->toonTableNeedsUpdate = false;
		}
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_0::SetupTexture(const POLY *thePoly, bool enableTexturing)
{
	OGLRenderRef &OGLRef = *this->ref;
	const PolygonTexParams params = thePoly->getTexParams();
	
	// Check if we need to use textures
	if (thePoly->texParam == 0 || params.texFormat == TEXMODE_NONE || !enableTexturing)
	{
		glUniform1i(OGLRef.uniformHasTexture, GL_FALSE);
		return OGLERROR_NOERR;
	}
	
	glUniform1i(OGLRef.uniformHasTexture, GL_TRUE);
	
	//	texCacheUnit.TexCache_SetTexture<TexFormat_32bpp>(format, texpal);
	TexCacheItem *newTexture = TexCache_SetTexture(TexFormat_32bpp, thePoly->texParam, thePoly->texPalette);
	if(newTexture != this->currTexture)
	{
		this->currTexture = newTexture;
		//has the ogl renderer initialized the texture?
		if(!this->currTexture->deleteCallback)
		{
			this->currTexture->deleteCallback = texDeleteCallback;
			
			if(OGLRef.freeTextureIDs.empty())
			{
				this->ExpandFreeTextures();
			}
			
			this->currTexture->texid = (u64)OGLRef.freeTextureIDs.front();
			OGLRef.freeTextureIDs.pop();
			
			glBindTexture(GL_TEXTURE_2D, (GLuint)this->currTexture->texid);
			
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (params.enableRepeatS ? (params.enableMirroredRepeatS ? GL_MIRRORED_REPEAT : GL_REPEAT) : GL_CLAMP_TO_EDGE));
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (params.enableRepeatT ? (params.enableMirroredRepeatT ? GL_MIRRORED_REPEAT : GL_REPEAT) : GL_CLAMP_TO_EDGE));
			
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
						 this->currTexture->sizeX, this->currTexture->sizeY, 0,
						 GL_RGBA, GL_UNSIGNED_BYTE, this->currTexture->decoded);
		}
		else
		{
			//otherwise, just bind it
			glBindTexture(GL_TEXTURE_2D, (GLuint)this->currTexture->texid);
		}
		
		glUniform2f(OGLRef.uniformTexScale, this->currTexture->invSizeX, this->currTexture->invSizeY);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_1::ReadBackPixels()
{
	const unsigned int i = this->doubleBufferIndex;
	
	this->DownsampleFBO();
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER, this->ref->pboRenderDataID[i]);
	glReadPixels(0, 0, 256, 192, GL_BGRA, GL_UNSIGNED_BYTE, 0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	
	this->gpuScreen3DHasNewData[i] = true;
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_2_1::RenderFinish()
{
	const unsigned int i = this->doubleBufferIndex;
	
	if (!this->gpuScreen3DHasNewData[i])
	{
		return OGLERROR_NOERR;
	}
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER, this->ref->pboRenderDataID[i]);
	
	const u32 *__restrict mappedBufferPtr = (u32 *__restrict)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
	if (mappedBufferPtr != NULL)
	{
		this->ConvertFramebuffer(mappedBufferPtr, (u32 *)gfx3d_convertedScreen);
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	}
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	
	this->gpuScreen3DHasNewData[i] = false;
	
	return OGLERROR_NOERR;
}
