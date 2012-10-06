/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2011 DeSmuME team

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

CACHE_ALIGN float material_8bit_to_float[255] = {0};

bool (*oglrender_init)() = 0;
bool (*oglrender_beginOpenGL)() = 0;
void (*oglrender_endOpenGL)() = 0;

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

static DS_ALIGN(16) u8  GPU_screen3D			[256*192*4];

static const unsigned short map3d_cull[4] = {GL_FRONT_AND_BACK, GL_FRONT, GL_BACK, 0};
static const int texEnv[4] = { GL_MODULATE, GL_DECAL, GL_MODULATE, GL_MODULATE };
static const int depthFunc[2] = { GL_LESS, GL_EQUAL };

//derived values extracted from polyattr etc
static bool wireframe=false, alpha31=false;
static unsigned int polyID=0;
static unsigned int depthFuncMode=0;
static unsigned int envMode=0;
static unsigned int lastEnvMode=0;
static unsigned int cullingMask=0;
static bool alphaDepthWrite;
static unsigned int lightMask=0;
static bool isTranslucent;

static u32 textureFormat=0, texturePalette=0;

static char *extString = NULL;
// ClearImage/Rear-plane (FBO)
GLenum	oglClearImageTextureID[2] = {0};	// 0 - image, 1 - depth
GLuint	oglClearImageBuffers = 0;
GLuint	oglClearImageRender[1] = {0};
bool	oglFBOdisabled = false;
u32		*oglClearImageColor = NULL;
float	*oglClearImageDepth = NULL;
u16		*oglClearImageColorTemp = NULL;
u16		*oglClearImageDepthTemp = NULL;
u32		oglClearImageScrollOld = 0;

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

#define NOSHADERS(s)					{ hasShaders = false; INFO("Shaders aren't supported on your system, using fixed pipeline\n(%s)\n", s); return; }

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

bool hasShaders = false;

GLuint vertexShaderID;
GLuint fragmentShaderID;
GLuint shaderProgram;

static GLint hasTexLoc;
static GLint texBlendLoc;
static GLint oglWBuffer;
static bool hasTexture = false;

static TexCacheItem* currTexture = NULL;

/* Shaders init */

static void createShaders()
{
	hasShaders = true;

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
	if(hasShaders)
	{
		glUniform1i(hasTexLoc, 0);
		hasTexture = false;
		glUniform1i(texBlendLoc, 0);
		glUniform1i(oglWBuffer, 0);
	}

	TexCache_Reset();
	if (currTexture) 
		delete currTexture;
	currTexture = NULL;

//	memset(GPU_screenStencil,0,sizeof(GPU_screenStencil));
	memset(GPU_screen3D,0,sizeof(GPU_screen3D));

	if (!oglFBOdisabled)
	{
		memset(oglClearImageColor, 0, 256*192*sizeof(u32));
		memset(oglClearImageDepth, 0, 256*192*sizeof(float));
		memset(oglClearImageColorTemp, 0, 256*192*sizeof(u16));
		memset(oglClearImageDepthTemp, 0, 256*192*sizeof(u16));
		oglClearImageScrollOld = 0;
	}
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
		material_8bit_to_float[i] = (float)(i<<2)/255.f;

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
#endif
#if !defined(GL_VERSION_1_3) || defined(_MSC_VER) || defined(__INTEL_COMPILER)
	INITOGLEXT(PFNGLACTIVETEXTUREPROC,glActiveTexture)
#endif

	/* Create the shaders */
	createShaders();

	/* Assign the texture units : 0 for main textures, 1 for toon table */
	/* Also init the locations for some variables in the shaders */
	if(hasShaders)
	{
		loc = glGetUniformLocation(shaderProgram, "tex2d");
		glUniform1i(loc, 0);

		loc = glGetUniformLocation(shaderProgram, "toonTable");
		glUniform1i(loc, 1);

		hasTexLoc = glGetUniformLocation(shaderProgram, "hasTexture");

		texBlendLoc = glGetUniformLocation(shaderProgram, "texBlending");

		oglWBuffer = glGetUniformLocation(shaderProgram, "oglWBuffer");
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

	if(hasShaders)
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

	// FBO
	oglFBOdisabled = (strstr(extString, "GL_ARB_framebuffer_object") == NULL)?true:false;

	if (!oglFBOdisabled)
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
			oglFBOdisabled = true;
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

	OGLReset();

	ENDGL();

	return 1;
}

static void OGLClose()
{
	if(!BEGINGL())
		return;

	if(hasShaders)
	{
		glUseProgram(0);

		glDetachShader(shaderProgram, vertexShaderID);
		glDetachShader(shaderProgram, fragmentShaderID);

		glDeleteProgram(shaderProgram);
		glDeleteShader(vertexShaderID);
		glDeleteShader(fragmentShaderID);

		hasShaders = false;
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

	// FBO
	if (!oglFBOdisabled)
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
}

static void texDeleteCallback(TexCacheItem* item)
{
	freeTextureIds.push((GLuint)item->texid);
	if(currTexture == item)
		currTexture = NULL;
}

static void setTexture(unsigned int format, unsigned int texpal)
{
	textureFormat = format;
	texturePalette = texpal;

	u32 textureMode = (unsigned short)((format>>26)&0x07);

	if (format==0)
	{
		if(hasShaders && hasTexture) { glUniform1i(hasTexLoc, 0); hasTexture = false; }
		return;
	}
	if (textureMode==0)
	{
		if(hasShaders && hasTexture) { glUniform1i(hasTexLoc, 0); hasTexture = false; }
		return;
	}

	if(hasShaders)
	{
		if(!hasTexture) { glUniform1i(hasTexLoc, 1); hasTexture = true; }
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
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glScalef(currTexture->invSizeX, currTexture->invSizeY, 1.0f);

	}
}



//controls states:
//glStencilFunc
//glStencilOp
//glColorMask
static u32 stencilStateSet = -1;

static u32 polyalpha=0;

static void BeginRenderPoly()
{
	bool enableDepthWrite = true;

	xglDepthFunc (depthFuncMode);

	// Cull face
	if (cullingMask == 0x03)
	{
		xglDisable(GL_CULL_FACE);
	}
	else
	{
		xglEnable(GL_CULL_FACE);
		glCullFace(map3d_cull[cullingMask]);
	}

	if (!wireframe)
	{
		xglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	}
	else
	{
		xglPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	}

	setTexture(textureFormat, texturePalette);

	if(isTranslucent)
		enableDepthWrite = alphaDepthWrite;

	//handle shadow polys
	if(envMode == 3)
	{
		xglEnable(GL_STENCIL_TEST);
		if(polyID == 0) {
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
		if(isTranslucent)
		{
			stencilStateSet = 3;
			glStencilFunc(GL_NOTEQUAL,polyID,255);
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

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, texEnv[envMode]);

	if(hasShaders)
	{
		if(envMode != lastEnvMode)
		{
			lastEnvMode = envMode;

			int _envModes[4] = {0, 1, (2 + gfx3d.renderState.shading), 0};
			glUniform1i(texBlendLoc, _envModes[envMode]);
		}
	}

	xglDepthMask(enableDepthWrite?GL_TRUE:GL_FALSE);
}

static void InstallPolygonAttrib(u32 val)
{
	// Light enable/disable
	lightMask = (val&0xF);

	// texture environment
	envMode = (val&0x30)>>4;

	// overwrite depth on alpha pass
	alphaDepthWrite = BIT11(val)!=0;

	// depth test function
	depthFuncMode = depthFunc[BIT14(val)];

	// back face culling
	cullingMask = (val >> 6) & 0x03;

	alpha31 = ((val>>16)&0x1F)==31;
	
	// Alpha value, actually not well handled, 0 should be wireframe
	wireframe = ((val>>16)&0x1F)==0;

	polyalpha = ((val>>16)&0x1F);

	// polyID
	polyID = (val>>24)&0x3F;
}

static void Control()
{
	if(gfx3d.renderState.enableTexturing) glEnable (GL_TEXTURE_2D);
	else glDisable (GL_TEXTURE_2D);

	if(gfx3d.renderState.enableAlphaTest)
		// FIXME: alpha test should pass gfx3d.alphaTestRef==poly->getAlpha
		glAlphaFunc	(GL_GREATER, gfx3d.renderState.alphaTestRef/31.f);
	else
		glAlphaFunc	(GL_GREATER, 0);

	if(gfx3d.renderState.enableAlphaBlending)
	{
		glEnable		(GL_BLEND);
	}
	else
	{
		glDisable		(GL_BLEND);
	}
}


static void GL_ReadFramebuffer()
{
	if(!BEGINGL()) return; 
	glFinish();
//	glReadPixels(0,0,256,192,GL_STENCIL_INDEX,		GL_UNSIGNED_BYTE,	GPU_screenStencil);
	glReadPixels(0,0,256,192,GL_BGRA_EXT,			GL_UNSIGNED_BYTE,	GPU_screen3D);	
	ENDGL();

	//convert the pixels to a different format which is more convenient
	//is it safe to modify the screen buffer? if not, we could make a temp copy
	for(int i=0,y=191;y>=0;y--)
	{
		u8* dst = gfx3d_convertedScreen + (y<<(8+2));

		for(int x=0;x<256;x++,i++)
		{
			u32 &u32screen3D = ((u32*)GPU_screen3D)[i];
			u32screen3D>>=2;
			u32screen3D &= 0x3F3F3F3F;
			
			const int t = i<<2;
			const u8 a = GPU_screen3D[t+3] >> 1;
			const u8 r = GPU_screen3D[t+2];
			const u8 g = GPU_screen3D[t+1];
			const u8 b = GPU_screen3D[t+0];
			*dst++ = r;
			*dst++ = g;
			*dst++ = b;
			*dst++ = a;
		}
	}

#if 0
	//convert the pixels to a different format which is more convenient
	//is it safe to modify the screen buffer? if not, we could make a temp copy
	for(int i=0,y=191;y>=0;y--)
	{
		u16* dst = gfx3d_convertedScreen + (y<<8);
		u8* dstAlpha = gfx3d_convertedAlpha + (y<<8);

			//I dont know much about this kind of stuff, but this seems to help
			//for some reason I couldnt make the intrinsics work 
			//u8* u8screen3D =  (u8*)&((u32*)GPU_screen3D)[i];
			/*#define PREFETCH32(X,Y) __asm { prefetchnta [u8screen3D+32*0x##X##Y] }
			#define PREFETCH128(X) 	PREFETCH32(X,0) PREFETCH32(X,1) PREFETCH32(X,2) PREFETCH32(X,3) \
									PREFETCH32(X,4) PREFETCH32(X,5) PREFETCH32(X,6) PREFETCH32(X,7) \
									PREFETCH32(X,8) PREFETCH32(X,9) PREFETCH32(X,A) PREFETCH32(X,B) \
									PREFETCH32(X,C) PREFETCH32(X,D) PREFETCH32(X,E) PREFETCH32(X,F) 
			PREFETCH128(0); PREFETCH128(1);*/

		for(int x=0;x<256;x++,i++)
		{
			u32 &u32screen3D = ((u32*)GPU_screen3D)[i];
			u32screen3D>>=3;
			u32screen3D &= 0x1F1F1F1F;

			const int t = i<<2;
			const u8 a = GPU_screen3D[t+3];
			const u8 r = GPU_screen3D[t+2];
			const u8 g = GPU_screen3D[t+1];
			const u8 b = GPU_screen3D[t+0];
			dst[x] = R5G5B5TORGB15(r,g,b) | alpha_lookup[a];
			dstAlpha[x] = a;
		}
	}
#endif
}

// TODO: optimize
// Tested:	Sonic Chronicles Dark Brotherhood
//			The Chronicles of Narnia - The Lion, The Witch and The Wardrobe
//			Harry Potter and the Order of the Phoenix
static void oglClearImageFBO()
{
	if (oglFBOdisabled) return;
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
	float alpha = 1.0f;

	if(!BEGINGL()) return;

	Control();

	if(hasShaders)
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

		glUniform1i(oglWBuffer, gfx3d.renderState.wbuffer);
	}

	xglDepthMask(GL_TRUE);

	glClearStencil((gfx3d.renderState.clearColor >> 24) & 0x3F);
	u32 clearFlag = GL_STENCIL_BUFFER_BIT;

	if (!oglFBOdisabled && gfx3d.renderState.enableClearImage)
			oglClearImageFBO();
	else
	{
		float clearColor[4] = {
			((float)(gfx3d.renderState.clearColor&0x1F))/31.0f,
			((float)((gfx3d.renderState.clearColor>>5)&0x1F))/31.0f,
			((float)((gfx3d.renderState.clearColor>>10)&0x1F))/31.0f,
			((float)((gfx3d.renderState.clearColor>>16)&0x1F))/31.0f,
		};
		glClearColor(clearColor[0],clearColor[1],clearColor[2],clearColor[3]);
		glClearDepth((float)gfx3d.renderState.clearDepth / (float)0x00FFFFFF);
		clearFlag |= GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
	}
	
	glClear(clearFlag);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();


	//render display list
	//TODO - properly doublebuffer the display lists
	{

		u32 lastTextureFormat = 0, lastTexturePalette = 0, lastPolyAttr = 0, lastViewport = 0xFFFFFFFF;
		// int lastProjIndex = -1;

		for(int i=0;i<gfx3d.polylist->count;i++) {
			POLY *poly = &gfx3d.polylist->list[gfx3d.indexlist.list[i]];
			int type = poly->type;

			//a very macro-level state caching approach:
			//these are the only things which control the GPU rendering state.
			if(i==0 || lastTextureFormat != poly->texParam || lastTexturePalette != poly->texPalette || lastPolyAttr != poly->polyAttr)
			{
				isTranslucent = poly->isTranslucent();
				InstallPolygonAttrib(poly->polyAttr);
				lastTextureFormat = textureFormat = poly->texParam;
				lastTexturePalette = texturePalette = poly->texPalette;
				lastPolyAttr = poly->polyAttr;
				BeginRenderPoly();
			}
			
			if(lastViewport != poly->viewport)
			{
				VIEWPORT viewport;
				viewport.decode(poly->viewport);
				glViewport(viewport.x,viewport.y,viewport.width,viewport.height);
				lastViewport = poly->viewport;
			}

			if(wireframe || !isTranslucent) alpha = 1.0f;
			else 
				alpha = poly->getAlpha()/31.0f;

			GLenum frm[] = {GL_TRIANGLES, GL_QUADS, GL_TRIANGLE_STRIP, GL_QUADS,	//TODO: GL_QUAD_STRIP
							GL_LINE_LOOP, GL_LINE_LOOP, GL_LINE_STRIP, GL_LINE_STRIP};

			glBegin(frm[poly->vtxFormat]);

			for(int j = 0; j < type; j++)
			{
				VERT *vert = &gfx3d.vertlist->list[poly->vertIndexes[j]];
				
				glTexCoord2fv(vert->texcoord);
				glColor4f(material_8bit_to_float[vert->color[0]], 
							material_8bit_to_float[vert->color[1]], 
							material_8bit_to_float[vert->color[2]],
							alpha);
				glVertex4fv(vert->coord);
			}

			glEnd();
		}
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

GPU3DInterface gpu3Dgl = {
	"OpenGL",
	OGLInit,
	OGLReset,
	OGLClose,
	OGLRender,
	OGLVramReconfigureSignal,
};
