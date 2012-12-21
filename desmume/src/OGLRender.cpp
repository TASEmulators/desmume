/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2012 DeSmuME team

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

#define VERT_INDEX_BUFFER_SIZE 8192

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
#else
#ifdef __APPLE__
	#include <OpenGL/gl.h>
	#include <OpenGL/glext.h>
#else
	#include <GL/gl.h>
	#include <GL/glext.h>
	/* This is a workaround needed to compile against nvidia GL headers */
	#ifndef GL_ALPHA_BLEND_EQUATION_ATI
	#undef GL_VERSION_1_3
	#endif
#endif
#endif

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

static DS_ALIGN(16) u8  GPU_screen3D			[256*192*4];

static const GLenum map3d_cull[4] = {GL_FRONT_AND_BACK, GL_FRONT, GL_BACK, 0};
static const GLint texEnv[4] = { GL_MODULATE, GL_DECAL, GL_MODULATE, GL_MODULATE };
static const GLenum depthFunc[2] = { GL_LESS, GL_EQUAL };

// Multithreading States
static bool enableMultithreading = false;
static bool isReadPixelsWorking = false;
static Task oglReadPixelsTask;

// Polygon Info
static PolygonAttributes currentPolyAttr;
static PolygonTexParams currentTexParams;
static GLenum depthFuncMode = 0;
static unsigned int lastEnvMode = 0;
static GLenum cullingMode = 0;
static GLfloat polyAlpha = 1.0f;

static u32 stencilStateSet = -1;

// OpenGL Feature Support
static char *extString = NULL;
static bool isVBOSupported = false;
static bool isPBOSupported = false;
static bool isFBOSupported = false;
static bool isShaderSupported = false;

// ClearImage/Rear-plane (FBO)
static GLenum oglClearImageTextureID[2] = {0};	// 0 - image, 1 - depth
static GLuint oglClearImageBuffers = 0;
static u32 *oglClearImageColor = NULL;
static float *oglClearImageDepth = NULL;
static u16 *oglClearImageColorTemp = NULL;
static u16 *oglClearImageDepthTemp = NULL;
static u32 oglClearImageScrollOld = 0;

// VBO
static GLuint vboVertexID;
static GLuint vboTexCoordID;

// PBO
static GLuint pboRenderDataID[2];
static u8 *pboRenderBuffer[2];

// Shader states
static GLuint vertexShaderID;
static GLuint fragmentShaderID;
static GLuint shaderProgram;

static GLint uniformPolyAlpha;
static GLint uniformTexScale;
static GLint uniformHasTexture;
static GLint uniformTextureBlendingMode;
static GLint uniformWBuffer;

static bool hasTexture = false;
static TexCacheItem* currTexture = NULL;

static GLfloat *color4fBuffer = NULL;
static GLushort *vertIndexBuffer = NULL;

static CACHE_ALIGN GLfloat material_8bit_to_float[255] = {0};
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
// VBO and PBO
OGLEXT(PFNGLGENBUFFERSPROC,glGenBuffersARB)
OGLEXT(PFNGLDELETEBUFFERSPROC,glDeleteBuffersARB)
OGLEXT(PFNGLBINDBUFFERPROC,glBindBufferARB)
OGLEXT(PFNGLBUFFERDATAPROC,glBufferDataARB)
OGLEXT(PFNGLMAPBUFFERPROC,glMapBufferARB)
OGLEXT(PFNGLUNMAPBUFFERPROC,glUnmapBufferARB)
// FBO
OGLEXT(PFNGLGENFRAMEBUFFERSEXTPROC,glGenFramebuffersEXT);
OGLEXT(PFNGLBINDFRAMEBUFFEREXTPROC,glBindFramebufferEXT);
OGLEXT(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC,glFramebufferRenderbufferEXT);
OGLEXT(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC,glFramebufferTexture2DEXT);
OGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC,glCheckFramebufferStatusEXT);
OGLEXT(PFNGLDELETEFRAMEBUFFERSEXTPROC,glDeleteFramebuffersEXT);
OGLEXT(PFNGLBLITFRAMEBUFFEREXTPROC,glBlitFramebufferEXT);
#endif

#if !defined(GL_VERSION_1_3) || defined(_MSC_VER) || defined(__INTEL_COMPILER)
OGLEXT(PFNGLACTIVETEXTUREPROC,glActiveTexture)
#endif

static void* execReadPixelsTask(void *arg)
{
	u8 *pixBuffer = NULL;
	
	if (isPBOSupported)
	{
		unsigned int *bufferIndex = (unsigned int *)arg;
		
		if(!BEGINGL()) return 0;
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboRenderDataID[*bufferIndex]);
		
		pboRenderBuffer[*bufferIndex] = (u8 *)glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
		if (pboRenderBuffer[*bufferIndex] != NULL)
		{
			glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
		}
		
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
		ENDGL();
		
		pixBuffer = pboRenderBuffer[*bufferIndex];
	}
	else
	{
		if(!BEGINGL()) return 0;
		glReadPixels(0, 0, 256, 192, GL_BGRA_EXT, GL_UNSIGNED_BYTE, GPU_screen3D);
		ENDGL();
		
		pixBuffer = GPU_screen3D;
	}
	
	//convert the pixels to a different format which is more convenient
	//is it safe to modify the screen buffer? if not, we could make a temp copy
	for(int i=0,y=191;y>=0;y--)
	{
		u8* dst = gfx3d_convertedScreen + (y<<(8+2));
		
		for(int x=0;x<256;x++,i++)
		{
			u32 &u32screen3D = ((u32*)pixBuffer)[i];
			u32screen3D>>=2;
			u32screen3D &= 0x3F3F3F3F;
			
			const int t = i<<2;
			const u8 a = pixBuffer[t+3] >> 1;
			const u8 r = pixBuffer[t+2];
			const u8 g = pixBuffer[t+1];
			const u8 b = pixBuffer[t+0];
			
			*dst++ = r;
			*dst++ = g;
			*dst++ = b;
			*dst++ = a;
		}
	}
	
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

static void xglPolygonMode(GLenum face,GLenum mode) {
	static GLenum oldmodes[2] = {-1,-1};
	switch(face) {
		case GL_FRONT: if(oldmodes[0]==mode) return; else glPolygonMode(GL_FRONT,oldmodes[0]=mode); return;
		case GL_BACK: if(oldmodes[1]==mode) return; else glPolygonMode(GL_BACK,oldmodes[1]=mode); return;
		case GL_FRONT_AND_BACK: if(oldmodes[0]==mode && oldmodes[1]==mode) return; else glPolygonMode(GL_FRONT_AND_BACK,oldmodes[0]=oldmodes[1]=mode);
	}
}

#if 0
#ifdef _WIN32
static void xglUseProgram(GLuint program) {
	if(!glUseProgram) return;
	static GLuint oldprogram = -1;
	if(oldprogram==program) return;
	glUseProgram(oldprogram=program);
} 
#else
#if 0 /* not used */
static void xglUseProgram(GLuint program) {
	(void)program;
	return;
}
#endif
#endif
#endif

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

static std::queue<GLuint> freeTextureIds;

GLenum			oglToonTableTextureID;

#define NOSHADERS(s)					{ isShaderSupported = false; INFO("Shaders aren't supported on your system, using fixed pipeline\n(%s)\n", s); return; }

#define SHADER_COMPCHECK(s, t)				{ \
	GLint status = GL_TRUE; \
	glGetShaderiv(s, GL_COMPILE_STATUS, &status); \
	if(status != GL_TRUE) \
	{ \
		GLint logSize; \
		GLchar *log; \
		glGetShaderiv(s, GL_INFO_LOG_LENGTH, &logSize); \
		log = new GLchar[logSize]; \
		glGetShaderInfoLog(s, logSize, &logSize, log); \
		INFO("SEVERE : FAILED TO COMPILE GL SHADER : %s\n", log); \
		delete[] log; \
		if(s)glDeleteShader(s); \
		NOSHADERS("Failed to compile the "t" shader."); \
	} \
}

#define PROGRAM_COMPCHECK(p, s1, s2)	{ \
	GLint status = GL_TRUE; \
	glGetProgramiv(p, GL_LINK_STATUS, &status); \
	if(status != GL_TRUE) \
	{ \
		GLint logSize; \
		GLchar *log; \
		glGetProgramiv(p, GL_INFO_LOG_LENGTH, &logSize); \
		log = new GLchar[logSize]; \
		glGetProgramInfoLog(p, logSize, &logSize, log); \
		INFO("SEVERE : FAILED TO LINK GL SHADER PROGRAM : %s\n", log); \
		delete[] log; \
		if(s1)glDeleteShader(s1); \
		if(s2)glDeleteShader(s2); \
		NOSHADERS("Failed to link the shader program."); \
	} \
}

/* Shaders init */

static void createShaders()
{
	isShaderSupported = true;

#ifdef HAVE_LIBOSMESA
	NOSHADERS("Shaders aren't supported by OSMesa.");
#endif

	/* This check is just plain wrong. */
	/* It will always pass if you've OpenGL 2.0 or later, */
	/* even if your GFX card doesn't support shaders. */
/*	if (glCreateShader == NULL ||  //use ==NULL instead of !func to avoid always true warnings for some systems
		glShaderSource == NULL ||
		glCompileShader == NULL ||
		glCreateProgram == NULL ||
		glAttachShader == NULL ||
		glLinkProgram == NULL ||
		glUseProgram == NULL ||
		glGetShaderInfoLog == NULL)
		NOSHADERS("Shaders aren't supported by your system.");*/

	if ((strstr(extString, "GL_ARB_shader_objects") == NULL) ||
		(strstr(extString, "GL_ARB_vertex_shader") == NULL) ||
		(strstr(extString, "GL_ARB_fragment_shader") == NULL))
		NOSHADERS("Shaders aren't supported by your system.");

	vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	if(!vertexShaderID)
		NOSHADERS("Failed to create the vertex shader.");

	glShaderSource(vertexShaderID, 1, (const GLchar**)&vertexShader, NULL);
	glCompileShader(vertexShaderID);
	SHADER_COMPCHECK(vertexShaderID, "vertex");

	fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if(!fragmentShaderID)
		NOSHADERS("Failed to create the fragment shader.");

	glShaderSource(fragmentShaderID, 1, (const GLchar**)&fragmentShader, NULL);
	glCompileShader(fragmentShaderID);
	SHADER_COMPCHECK(fragmentShaderID, "fragment");

	shaderProgram = glCreateProgram();
	if(!shaderProgram)
		NOSHADERS("Failed to create the shader program.");

	glAttachShader(shaderProgram, vertexShaderID);
	glAttachShader(shaderProgram, fragmentShaderID);

	glLinkProgram(shaderProgram);
	PROGRAM_COMPCHECK(shaderProgram, vertexShaderID, fragmentShaderID);

	glValidateProgram(shaderProgram);
	glUseProgram(shaderProgram);

	INFO("Successfully created OpenGL shaders.\n");
}

//=================================================

static void OGLReset()
{
	if(isShaderSupported)
	{
		hasTexture = false;
		
		if(BEGINGL())
		{
			glUniform1f(uniformPolyAlpha, 1.0f);
			glUniform2f(uniformTexScale, 1.0f, 1.0f);
			glUniform1i(uniformHasTexture, 0);
			glUniform1i(uniformTextureBlendingMode, 0);
			glUniform1i(uniformWBuffer, 0);
			
			ENDGL();
		}
	}
	else
	{
		memset(color4fBuffer, 0, VERTLIST_SIZE * 4 * sizeof(GLfloat));
	}
	
	TexCache_Reset();
	if (currTexture) 
		delete currTexture;
	currTexture = NULL;

	memset(GPU_screen3D,0,sizeof(GPU_screen3D));

	if (isFBOSupported)
	{
		memset(oglClearImageColor, 0, 256*192*sizeof(u32));
		memset(oglClearImageDepth, 0, 256*192*sizeof(float));
		memset(oglClearImageColorTemp, 0, 256*192*sizeof(u16));
		memset(oglClearImageDepthTemp, 0, 256*192*sizeof(u16));
		oglClearImageScrollOld = 0;
	}
	
	memset(vertIndexBuffer, 0, VERT_INDEX_BUFFER_SIZE * sizeof(GLushort));
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

static char OGLInit(void)
{
	GLint loc = 0;

	if(!oglrender_init)
		return 0;
	if(!oglrender_init())
		return 0;

	if(!BEGINGL())
		return 0;

	for (u8 i = 0; i < 255; i++)
		material_8bit_to_float[i] = (GLfloat)(i<<2)/255.f;

	extString = (char*)glGetString(GL_EXTENSIONS);

	expandFreeTextures();

	glPixelStorei(GL_PACK_ALIGNMENT,8);

	xglEnable		(GL_NORMALIZE);
	xglEnable		(GL_DEPTH_TEST);
	glEnable		(GL_TEXTURE_1D);
	glEnable		(GL_TEXTURE_2D);

	glAlphaFunc		(GL_GREATER, 0);
	xglEnable		(GL_ALPHA_TEST);

	glViewport(0, 0, 256, 192);
	if (glGetError() != GL_NO_ERROR)
		return 0;

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
	// VBO and PBO
	INITOGLEXT(PFNGLGENBUFFERSPROC,glGenBuffersARB)
	INITOGLEXT(PFNGLDELETEBUFFERSPROC,glDeleteBuffersARB)
	INITOGLEXT(PFNGLBINDBUFFERPROC,glBindBufferARB)
	INITOGLEXT(PFNGLBUFFERDATAPROC,glBufferDataARB)
	INITOGLEXT(PFNGLMAPBUFFERPROC,glMapBufferARB)
	INITOGLEXT(PFNGLUNMAPBUFFERPROC,glUnmapBufferARB)
	// FBO
	INITOGLEXT(PFNGLGENFRAMEBUFFERSEXTPROC,glGenFramebuffersEXT);
	INITOGLEXT(PFNGLBINDFRAMEBUFFEREXTPROC,glBindFramebufferEXT);
	INITOGLEXT(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC,glFramebufferRenderbufferEXT);
	INITOGLEXT(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC,glFramebufferTexture2DEXT);
	INITOGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC,glCheckFramebufferStatusEXT);
	INITOGLEXT(PFNGLDELETEFRAMEBUFFERSEXTPROC,glDeleteFramebuffersEXT);
	INITOGLEXT(PFNGLBLITFRAMEBUFFEREXTPROC,glBlitFramebufferEXT);
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

	/* Create the shaders */
	createShaders();

	/* Assign the texture units : 0 for main textures, 1 for toon table */
	/* Also init the locations for some variables in the shaders */
	if(isShaderSupported)
	{
		loc = glGetUniformLocation(shaderProgram, "tex2d");
		glUniform1i(loc, 0);

		loc = glGetUniformLocation(shaderProgram, "toonTable");
		glUniform1i(loc, 1);
		
		uniformPolyAlpha = glGetUniformLocation(shaderProgram, "polyAlpha");
		uniformTexScale = glGetUniformLocation(shaderProgram, "texScale");
		uniformHasTexture = glGetUniformLocation(shaderProgram, "hasTexture");
		uniformTextureBlendingMode = glGetUniformLocation(shaderProgram, "texBlending");
		uniformWBuffer = glGetUniformLocation(shaderProgram, "oglWBuffer");
	}

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
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_DST_ALPHA);
		
	}
	
	// VBO
	isVBOSupported = (strstr(extString, "GL_ARB_vertex_buffer_object") == NULL)?false:true;
	if (isVBOSupported)
	{
		glGenBuffersARB(1, &vboVertexID);
		glGenBuffersARB(1, &vboTexCoordID);
	}
	
	// PBO
	isPBOSupported = (strstr(extString, "GL_ARB_pixel_buffer_object") == NULL)?false:true;
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

	if(isShaderSupported)
	{
		glGenTextures (1, &oglToonTableTextureID);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_1D, oglToonTableTextureID);
		glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP); //clamp so that we dont run off the edges due to 1.0 -> [0,31] math

		// Restore Toon table
		u32 rgbToonTable[32];
		for(int i=0;i<32;i++)
			rgbToonTable[i] = RGB15TO32_NOALPHA(gfx3d.renderState.u16ToonTable[i]);
		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, &rgbToonTable[0]);
		gfx3d.state.invalidateToon = false;
	}
	else
	{
		// Map the vertex list's colors with 4 floats per color. This is being done
		// because OpenGL needs 4-colors per vertex to support translucency. (The DS
		// uses 3-colors per vertex, and adds alpha through the poly, so we can't
		// simply reference the colors+alpha from just the vertices themselves.)
		color4fBuffer = new GLfloat[VERTLIST_SIZE * 4];
	}

	// FBO
	isFBOSupported = (strstr(extString, "GL_ARB_framebuffer_object") == NULL)?false:true;
	if (isFBOSupported)
	{
		// ClearImage/Rear-plane
		glGenTextures (2, &oglClearImageTextureID[0]);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, oglClearImageTextureID[0]);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		glBindTexture(GL_TEXTURE_2D, oglClearImageTextureID[1]);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 256, 192, 0,  GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

		// FBO - init
		glGenFramebuffersEXT(1, &oglClearImageBuffers);

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, oglClearImageBuffers);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, oglClearImageTextureID[0], 0);

		//glDrawBuffer(GL_NONE);
		//glReadBuffer(GL_NONE);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, oglClearImageTextureID[1], 0);
		
		if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT)
			INFO("Successfully created OpenGL Framebuffer object (FBO)\n");
		else
		{
			INFO("Failed to created OpenGL Framebuffer objects (FBO): ClearImage emulation disabled\n");
			isFBOSupported = false;
		}

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

		oglClearImageColor = new u32[256*192];
		oglClearImageColorTemp = new u16[256*192];
		oglClearImageDepth = new float[256*192];
		oglClearImageDepthTemp = new u16[256*192];
	}
	else
		INFO("OpenGL: graphics card not supports Framebuffer objects (FBO) - ClearImage emulation disabled\n");

	glActiveTexture(GL_TEXTURE0);

	ENDGL();
	
	// Set up multithreading
	isReadPixelsWorking = false;

	if (CommonSettings.num_cores > 1)
	{
#ifdef _WINDOWS
		if (!isPBOSupported)
		{
			// Don't know why this doesn't work on Windows when the GPU
			// lacks PBO support. Someone please research.
			enableMultithreading = false;
		}
		else
		{
			enableMultithreading = true;
			oglReadPixelsTask.start(false);
		}
#else	
		enableMultithreading = true;
		oglReadPixelsTask.start(false);
#endif
	}
	else
	{
		enableMultithreading = false;
	}
	
	// Maintain our own vertex index buffer for vertex batching and primitive
	// conversions. Such conversions are necessary since OpenGL deprecates
	// primitives like GL_QUADS and GL_QUAD_STRIP in later versions.
	vertIndexBuffer = new GLushort[VERT_INDEX_BUFFER_SIZE];
	
	OGLReset();

	return 1;
}

static void OGLClose()
{
	delete [] vertIndexBuffer;
	vertIndexBuffer = NULL;
	
	if(!BEGINGL())
		return;

	if(isShaderSupported)
	{
		glUseProgram(0);

		glDetachShader(shaderProgram, vertexShaderID);
		glDetachShader(shaderProgram, fragmentShaderID);

		glDeleteProgram(shaderProgram);
		glDeleteShader(vertexShaderID);
		glDeleteShader(fragmentShaderID);

		isShaderSupported = false;
	}
	else
	{
		delete [] color4fBuffer;
		color4fBuffer = NULL;
	}

	//kill the tex cache to free all the texture ids
	TexCache_Reset();

	while(!freeTextureIds.empty())
	{
		GLuint temp = freeTextureIds.front();
		freeTextureIds.pop();
		glDeleteTextures(1,&temp);
	}
	//glDeleteTextures(MAX_TEXTURE, &oglTempTextureID[0]);
	glDeleteTextures(1, &oglToonTableTextureID);

	if (isVBOSupported)
	{
		glDeleteBuffersARB(1, &vboVertexID);
		glDeleteBuffersARB(1, &vboTexCoordID);
	}
	
	if (isPBOSupported)
	{
		glDeleteBuffersARB(2, pboRenderDataID);
		pboRenderBuffer[0] = NULL;
		pboRenderBuffer[1] = NULL;
	}
	
	// FBO
	if (isFBOSupported)
	{
		glDeleteTextures(2, &oglClearImageTextureID[0]);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &oglClearImageBuffers);

		if (oglClearImageColor)
		{
			delete [] oglClearImageColor;
			oglClearImageColor = NULL;
		}

		if (oglClearImageDepth)
		{
			delete [] oglClearImageDepth;
			oglClearImageDepth = NULL;
		}

		if (oglClearImageColorTemp)
		{
			delete [] oglClearImageColorTemp;
			oglClearImageColorTemp = NULL;
		}

		if (oglClearImageDepthTemp)
		{
			delete [] oglClearImageDepthTemp;
			oglClearImageDepthTemp = NULL;
		}
	}

	ENDGL();
	
	if (enableMultithreading)
	{
		oglReadPixelsTask.finish();
		oglReadPixelsTask.shutdown();
		isReadPixelsWorking = false;
	}
}

static void texDeleteCallback(TexCacheItem* item)
{
	freeTextureIds.push((GLuint)item->texid);
	if(currTexture == item)
		currTexture = NULL;
}

static void setTexture(unsigned int format, unsigned int texpal)
{
	if (format == 0 || currentTexParams.texFormat == TEXMODE_NONE)
	{
		if(isShaderSupported && hasTexture) { glUniform1i(uniformHasTexture, 0); hasTexture = false; }
		return;
	}

	if(isShaderSupported)
	{
		if(!hasTexture) { glUniform1i(uniformHasTexture, 1); hasTexture = true; }
		glActiveTexture(GL_TEXTURE0);
	}


//	texCacheUnit.TexCache_SetTexture<TexFormat_32bpp>(format, texpal);
	TexCacheItem* newTexture = TexCache_SetTexture(TexFormat_32bpp,format,texpal);
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

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (BIT16(currTexture->texformat) ? (BIT18(currTexture->texformat)?GL_MIRRORED_REPEAT:GL_REPEAT) : GL_CLAMP));
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (BIT17(currTexture->texformat) ? (BIT19(currTexture->texformat)?GL_MIRRORED_REPEAT:GL_REPEAT) : GL_CLAMP));

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

static void SetupPolygonRender(POLY *thePoly)
{
	bool enableDepthWrite = true;
	
	// Get polygon info
	currentPolyAttr = thePoly->getAttributes();
	
	polyAlpha		= 1.0f;
	if (!currentPolyAttr.isWireframe && currentPolyAttr.isTranslucent)
	{
		polyAlpha	= divide5bitBy31LUT[currentPolyAttr.alpha];
	}
	
	if (isShaderSupported)
	{
		glUniform1f(uniformPolyAlpha, polyAlpha);
	}
	
	cullingMode		= map3d_cull[currentPolyAttr.surfaceCullingMode];	
	depthFuncMode	= depthFunc[currentPolyAttr.enableDepthTest];
	
	// Set up texture
	if (gfx3d.renderState.enableTexturing)
	{
		currentTexParams = thePoly->getTexParams();
		setTexture(thePoly->texParam, thePoly->texPalette);
	}
	
	// Set up rendering states
	xglDepthFunc (depthFuncMode);

	// Cull face
	if (cullingMode == 0)
	{
		xglDisable(GL_CULL_FACE);
	}
	else
	{
		xglEnable(GL_CULL_FACE);
		glCullFace(cullingMode);
	}

	if(currentPolyAttr.isTranslucent)
		enableDepthWrite = currentPolyAttr.enableAlphaDepthWrite;

	//handle shadow polys
	if(currentPolyAttr.polygonMode == 3)
	{
		xglEnable(GL_STENCIL_TEST);
		if(currentPolyAttr.polygonID == 0) {
			enableDepthWrite = false;
			if(stencilStateSet!=0) {
				stencilStateSet = 0;
				//when the polyID is zero, we are writing the shadow mask.
				//set stencilbuf = 1 where the shadow volume is obstructed by geometry.
				//do not write color or depth information.
				glStencilFunc(GL_ALWAYS,65,255);
				glStencilOp(GL_KEEP,GL_REPLACE,GL_KEEP);
				glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
			}
		} else {
			enableDepthWrite = true;
			if(stencilStateSet!=1) {
				stencilStateSet = 1;
				//when the polyid is nonzero, we are drawing the shadow poly.
				//only draw the shadow poly where the stencilbuf==1.
				//I am not sure whether to update the depth buffer here--so I chose not to.
				glStencilFunc(GL_EQUAL,65,255);
				glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
				glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
			}
		}
	} else {
		xglEnable(GL_STENCIL_TEST);
		if(currentPolyAttr.isTranslucent)
		{
			stencilStateSet = 3;
			glStencilFunc(GL_NOTEQUAL,currentPolyAttr.polygonID,255);
			glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
			glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
		}
		else
		if(stencilStateSet!=2) {
			stencilStateSet=2;	
			glStencilFunc(GL_ALWAYS,64,255);
			glStencilOp(GL_REPLACE,GL_REPLACE,GL_REPLACE);
			glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
		}
	}
	
	if(currentPolyAttr.polygonMode != lastEnvMode)
	{
		lastEnvMode = currentPolyAttr.polygonMode;
		
		if(isShaderSupported)
		{
			int _envModes[4] = {0, 1, (2 + gfx3d.renderState.shading), 0};
			glUniform1i(uniformTextureBlendingMode, _envModes[currentPolyAttr.polygonMode]);
		}
		else
		{
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, texEnv[currentPolyAttr.polygonMode]);
		}
	}

	xglDepthMask(enableDepthWrite?GL_TRUE:GL_FALSE);
}

static void Control()
{
	if (gfx3d.renderState.enableTexturing)
	{
		if (isShaderSupported)
		{
			glUniform1i(uniformHasTexture, 1);
		}
		else
		{
			glEnable(GL_TEXTURE_2D);
		}
	}
	else
	{
		if (isShaderSupported)
		{
			glUniform1i(uniformHasTexture, 0);
		}
		else
		{
			glDisable(GL_TEXTURE_2D);
		}
	}
	
	if(gfx3d.renderState.enableAlphaTest && (gfx3d.renderState.alphaTestRef > 0))
	{
		glAlphaFunc(GL_GEQUAL, divide5bitBy31LUT[gfx3d.renderState.alphaTestRef]);
	}
	else
	{
		glAlphaFunc(GL_GREATER, 0);
	}

	if(gfx3d.renderState.enableAlphaBlending)
	{
		glEnable(GL_BLEND);
	}
	else
	{
		glDisable(GL_BLEND);
	}
}

static void GL_ReadFramebuffer()
{
	static unsigned int bufferIndex = 0;
	
	bufferIndex = (bufferIndex + 1) % 2;
	
	if (isPBOSupported)
	{
		if(!BEGINGL()) return;
		
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboRenderDataID[bufferIndex]);
		glReadPixels(0, 0, 256, 192, GL_BGRA_EXT, GL_UNSIGNED_BYTE, 0);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
		
		ENDGL();
	}
	
	// If multithreading is enabled, call glReadPixels() on a separate thread
	// (or glMapBuffer()/glUnmapBuffer() if PBOs are supported). This is a big
	// deal, since these functions can cause the thread to block. If 3D rendering
	// is happening on the same thread as the core emulation, (which is the most
	// likely scenario), this can make the thread stall.
	//
	// We can get away with doing this since 3D rendering begins on H-Start,
	// but the emulation doesn't actually need the rendered data until H-Blank.
	// So in between that time, we can let these functions block the other thread
	// and then only block this thread for the remaining time difference.
	if (enableMultithreading)
	{
		isReadPixelsWorking = true;
		oglReadPixelsTask.execute(execReadPixelsTask, &bufferIndex);
	}
	else
	{
		execReadPixelsTask(&bufferIndex);
	}
}

// TODO: optimize
// Tested:	Sonic Chronicles Dark Brotherhood
//			The Chronicles of Narnia - The Lion, The Witch and The Wardrobe
//			Harry Potter and the Order of the Phoenix
static void oglClearImageFBO()
{
	if (!isFBOSupported) return;
	//printf("enableClearImage\n");
	u16* clearImage = (u16*)MMU.texInfo.textureSlotAddr[2];
	u16* clearDepth = (u16*)MMU.texInfo.textureSlotAddr[3];
	u16 scroll = T1ReadWord(MMU.ARM9_REG,0x356); //CLRIMAGE_OFFSET

	if ((oglClearImageScrollOld != scroll)||
		(memcmp(clearImage, oglClearImageColorTemp, 256*192*2) != 0) ||
			(memcmp(clearDepth, oglClearImageDepthTemp, 256*192*2) != 0))
	{
		oglClearImageScrollOld = scroll;
		memcpy(oglClearImageColorTemp, clearImage, 256*192*2);
		memcpy(oglClearImageDepthTemp, clearDepth, 256*192*2);
		
		u16 xscroll = scroll&0xFF;
		u16 yscroll = (scroll>>8)&0xFF;

		u32 dd = 256*192-256;
		for(int iy=0;iy<192;iy++) 
		{
			int y = ((iy + yscroll)&255)<<8;
			for(int ix=0;ix<256;ix++)
			{
				int x = (ix + xscroll)&255;
				int adr = y + x;
				
				u16 col = clearImage[adr];
				oglClearImageColor[dd] = RGB15TO32(col,255*(col>>15));
				
				u16 depth = clearDepth[adr] & 0x7FFF;

				oglClearImageDepth[dd] = (float)gfx3d_extendDepth_15_to_24(depth) / (float)0x00FFFFFF;
				dd++;
			}
			dd-=256*2;
		}

		// color
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, oglClearImageTextureID[0]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0,  GL_RGBA, GL_UNSIGNED_BYTE, &oglClearImageColor[0]);

		// depth
		glBindTexture(GL_TEXTURE_2D, oglClearImageTextureID[1]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 256, 192, 0,  GL_DEPTH_COMPONENT, GL_FLOAT, &oglClearImageDepth[0]);

		glActiveTexture(GL_TEXTURE0);
	}
	

	// color & depth
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, oglClearImageBuffers);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
	glBlitFramebufferEXT(0,0,256,192,0,0,256,192,GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT,GL_LINEAR);

	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);

}

static void OGLRender()
{
	static const GLenum frm[]	= {GL_TRIANGLES, GL_QUADS, GL_TRIANGLE_STRIP, GL_QUAD_STRIP,
								   GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_STRIP, GL_LINE_STRIP};
	
	if(!BEGINGL()) return;

	Control();

	if(isShaderSupported)
	{
		//NOTE: this toon invalidation logic is hopelessly buggy.
		//it may sometimes fail. it would be better to always recreate this data.
		//but, that may be slow. since the cost of uploading that texture is huge in opengl (relative to rasterizer).
		//someone please study it.
		//here is a suggestion: it may make sense to memcmp the toon tables and upload only when it actually changes
		if (gfx3d.renderState.invalidateToon)
		{
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_1D, oglToonTableTextureID);

			u32 rgbToonTable[32];
			for(int i=0;i<32;i++)
				rgbToonTable[i] = RGB15TO32_NOALPHA(gfx3d.renderState.u16ToonTable[i]);
			glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, &rgbToonTable[0]);
			gfx3d.state.invalidateToon = false;
		}

		glUniform1i(uniformWBuffer, gfx3d.renderState.wbuffer);
	}

	xglDepthMask(GL_TRUE);

	glClearStencil((gfx3d.renderState.clearColor >> 24) & 0x3F);
	u32 clearFlag = GL_STENCIL_BUFFER_BIT;

	if (isFBOSupported && gfx3d.renderState.enableClearImage)
			oglClearImageFBO();
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
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);
		
		if (isVBOSupported)
		{
			if (!isShaderSupported)
			{
				glBindBufferARB(GL_ARRAY_BUFFER, 0);
				glColorPointer(4, GL_FLOAT, 0, color4fBuffer);
			}
			
			glBindBufferARB(GL_ARRAY_BUFFER, vboVertexID);
			glBufferDataARB(GL_ARRAY_BUFFER, sizeof(VERT) * gfx3d.vertlist->count, gfx3d.vertlist, GL_STREAM_DRAW);
			glTexCoordPointer(2, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
			glVertexPointer(4, GL_FLOAT, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
			
			if (isShaderSupported)
			{
				glColorPointer(3, GL_UNSIGNED_BYTE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
			}
		}
		else
		{
			glTexCoordPointer(2, GL_FLOAT, sizeof(VERT), &gfx3d.vertlist->list[0].texcoord);
			glVertexPointer(4, GL_FLOAT, sizeof(VERT), &gfx3d.vertlist->list[0].coord);
			
			if (isShaderSupported)
			{
				glColorPointer(3, GL_UNSIGNED_BYTE, sizeof(VERT), &gfx3d.vertlist->list[0].color);
			}
			else
			{
				glColorPointer(4, GL_FLOAT, 0, color4fBuffer);
			}
		}
		
		// Render all polygons
		for(unsigned int i = 0; i < polyCount; i++)
		{
			POLY *poly = &gfx3d.polylist->list[gfx3d.indexlist.list[i]];
			polyPrimitive = frm[poly->vtxFormat];
			polyType = poly->type;

			//a very macro-level state caching approach:
			//these are the only things which control the GPU rendering state.
			if(lastPolyAttr != poly->polyAttr || lastTexParams != poly->texParam || lastTexPalette != poly->texPalette || i == 0)
			{
				lastPolyAttr	= poly->polyAttr;
				lastTexParams	= poly->texParam;
				lastTexPalette	= poly->texPalette;
				
				SetupPolygonRender(poly);
			}
			
			if(lastViewport != poly->viewport)
			{
				VIEWPORT viewport;
				viewport.decode(poly->viewport);
				glViewport(viewport.x,viewport.y,viewport.width,viewport.height);
				lastViewport = poly->viewport;
			}
			
			// Set up vertices
			if (isShaderSupported)
			{
				for(unsigned int j = 0; j < polyType; j++)
				{
					GLushort vertIndex = poly->vertIndexes[j];
					
					// While we're looping through our vertices, add each vertex index to
					// a buffer. For GL_QUADS and GL_QUAD_STRIP, we also add additional vertices
					// here to convert them to GL_TRIANGLES, which are much easier to work with
					// and won't be deprecated in future OpenGL versions.
					vertIndexBuffer[vertIndexCount++] = vertIndex;
					if (polyPrimitive == GL_QUADS || polyPrimitive == GL_QUAD_STRIP)
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
					GLushort vertIndex = poly->vertIndexes[j];
					GLushort colorIndex = vertIndex * 4;
					
					// Consolidate the vertex color and the poly alpha to our internal color buffer
					// so that OpenGL can use it.
					VERT *vert = &gfx3d.vertlist->list[vertIndex];
					color4fBuffer[colorIndex+0] = material_8bit_to_float[vert->color[0]];
					color4fBuffer[colorIndex+1] = material_8bit_to_float[vert->color[1]];
					color4fBuffer[colorIndex+2] = material_8bit_to_float[vert->color[2]];
					color4fBuffer[colorIndex+3] = polyAlpha;
					
					// While we're looping through our vertices, add each vertex index to
					// a buffer. For GL_QUADS and GL_QUAD_STRIP, we also add additional vertices
					// here to convert them to GL_TRIANGLES, which are much easier to work with
					// and won't be deprecated in future OpenGL versions.
					vertIndexBuffer[vertIndexCount++] = vertIndex;
					if (polyPrimitive == GL_QUADS || polyPrimitive == GL_QUAD_STRIP)
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
				POLY *nextPoly = &gfx3d.polylist->list[gfx3d.indexlist.list[i+1]];
				
				if (lastPolyAttr == nextPoly->polyAttr &&
					lastTexParams == nextPoly->texParam &&
					lastTexPalette == nextPoly->texPalette &&
					polyPrimitive == frm[nextPoly->vtxFormat] &&
					polyPrimitive != GL_LINE_LOOP &&
					polyPrimitive != GL_LINE_STRIP &&
					frm[nextPoly->vtxFormat] != GL_LINE_LOOP &&
					frm[nextPoly->vtxFormat] != GL_LINE_STRIP)
				{
					needVertexUpload = false;
				}
			}
			
			// Upload the vertices if necessary.
			if (needVertexUpload)
			{
				// In wireframe mode, redefine all primitives as GL_LINE_LOOP rather than
				// setting the polygon mode to GL_LINE though glPolygonMode(). Not only is
				// drawing more accurate this way, but it also allows GL_QUADS and
				// GL_QUAD_STRIP primitives to properly draw as wireframe without the
				// extra diagonal line.
				if (currentPolyAttr.isWireframe)
				{
					polyPrimitive = GL_LINE_LOOP;
				}
				else if (polyPrimitive == GL_TRIANGLE_STRIP || polyPrimitive == GL_QUADS || polyPrimitive == GL_QUAD_STRIP)
				{
					// Redefine GL_QUADS and GL_QUAD_STRIP as GL_TRIANGLES since we converted them.
					//
					// Also redefine GL_TRIANGLE_STRIP as GL_TRIANGLES. This is okay since this
					// is actually how the POLY struct stores triangle strip vertices, which is
					// in sets of 3 vertices each. This redefinition is necessary since uploading
					// more than 3 indices at a time will cause glDrawElements() to draw the
					// triangle strip incorrectly.
					polyPrimitive = GL_TRIANGLES;
				}
				
				// Upload the vertices to the framebuffer.
				glDrawElements(polyPrimitive, vertIndexCount, GL_UNSIGNED_SHORT, vertIndexBuffer);
				vertIndexCount = 0;
			}
		}
		
		if (isVBOSupported)
		{
			glBindBufferARB(GL_ARRAY_BUFFER, 0);
		}
		
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	
	//needs to happen before endgl because it could free some textureids for expired cache items
	TexCache_EvictFrame();

	ENDGL();

	GL_ReadFramebuffer();
}

static void OGLVramReconfigureSignal()
{
	TexCache_Invalidate();
}

static u8* OGLGetLineData(u8 lineNumber)
{
	// If OpenGL is still reading back pixels on a separate thread, wait for it to finish.
	if (isReadPixelsWorking)
	{
		oglReadPixelsTask.finish();
		isReadPixelsWorking = false;
	}
	
	return ( gfx3d_convertedScreen + (lineNumber << (8+2)) );
}

GPU3DInterface gpu3Dgl = {
	"OpenGL",
	OGLInit,
	OGLReset,
	OGLClose,
	OGLRender,
	OGLVramReconfigureSignal,
	OGLGetLineData
};
