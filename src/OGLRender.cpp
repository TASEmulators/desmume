/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

//problem - alpha-on-alpha texture rendering might work but the dest alpha buffer isnt tracked correctly
//due to zeromus not having any idea how to set dest alpha blending in opengl.
//so, it doesnt composite to 2d correctly.
//(re: new super mario brothers renders the stormclouds at the beginning)

#include "OGLRender.h"
#include "debug.h"

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

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <GL/gl.h>
	#include <GL/glext.h>
#else
#ifdef DESMUME_COCOA
	#include <OpenGL/gl.h>
	#include <OpenGL/glext.h>
#else
	#include <GL/gl.h>
	#include <GL/glext.h>
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



#ifndef CTASSERT
#define	CTASSERT(x)		typedef char __assert ## y[(x) ? 1 : -1]
#endif

static ALIGN(16) u8  GPU_screen3D			[256*192*4];


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

//------------------------------------------------------------

#define OGLEXT(x,y) x y = 0;

#ifdef _WIN32
#define INITOGLEXT(x,y) y = (x)wglGetProcAddress(#y);
#elif !defined(DESMUME_COCOA)
#include <GL/glx.h>
#define INITOGLEXT(x,y) y = (x)glXGetProcAddress((const GLubyte *) #y);
#endif

#ifndef DESMUME_COCOA
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
OGLEXT(PFNGLBLENDFUNCSEPARATEEXTPROC,glBlendFuncSeparateEXT)
OGLEXT(PFNGLGETUNIFORMLOCATIONPROC,glGetUniformLocation)
OGLEXT(PFNGLUNIFORM1IPROC,glUniform1i)
OGLEXT(PFNGLUNIFORM1IVPROC,glUniform1iv)
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



GLenum			oglTempTextureID[MAX_TEXTURE];
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
		delete log; \
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
		delete log; \
		if(s1)glDeleteShader(s1); \
		if(s2)glDeleteShader(s2); \
		NOSHADERS("Failed to link the shader program."); \
	} \
}

bool hasShaders = false;

/* Vertex shader */
GLuint vertexShaderID;
/* Fragment shader */
GLuint fragmentShaderID;
/* Shader program */
GLuint shaderProgram;

static GLuint hasTexLoc;
static GLuint texBlendLoc;
static bool hasTexture = false;

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

	const char *extString = (const char*)glGetString(GL_EXTENSIONS);
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
		
	}

	TexCache_Reset();

	for (int i = 0; i < MAX_TEXTURE; i++)
		texcache[i].id=oglTempTextureID[i];

//	memset(GPU_screenStencil,0,sizeof(GPU_screenStencil));
	memset(GPU_screen3D,0,sizeof(GPU_screen3D));
}




static void BindTexture(u32 tx)
{
	glBindTexture(GL_TEXTURE_2D,(GLuint)texcache[tx].id);
	glMatrixMode (GL_TEXTURE);
	glLoadIdentity ();
	glScaled (texcache[tx].invSizeX, texcache[tx].invSizeY, 1.0f);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (BIT16(texcache[tx].frm) ? (BIT18(texcache[tx].frm)?GL_MIRRORED_REPEAT:GL_REPEAT) : GL_CLAMP));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (BIT17(texcache[tx].frm) ? (BIT19(texcache[tx].frm)?GL_MIRRORED_REPEAT:GL_REPEAT) : GL_CLAMP));
}

static void BindTextureData(u32 tx, u8* data)
{
	BindTexture(tx);

#if 0
	for (int i=0; i < texcache[tx].sizeX * texcache[tx].sizeY*4; i++)
		data[i] = 0xFF;
#endif
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
		texcache[tx].sizeX, texcache[tx].sizeY, 0, 
		GL_RGBA, GL_UNSIGNED_BYTE, data);
}


static char OGLInit(void)
{
	GLuint loc = 0;

	if(!oglrender_init)
		return 0;
	if(!oglrender_init())
		return 0;

	if(!BEGINGL())
		return 0;

	TexCache_BindTexture = BindTexture;
	TexCache_BindTextureData = BindTextureData;
	glGenTextures (MAX_TEXTURE, &oglTempTextureID[0]);

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

#ifndef DESMUME_COCOA
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
#ifdef HAVE_LIBOSMESA
	glBlendFuncSeparateEXT = NULL;
#else
	INITOGLEXT(PFNGLBLENDFUNCSEPARATEEXTPROC,glBlendFuncSeparateEXT)
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
	}

	//we want to use alpha destination blending so we can track the last-rendered alpha value
	if(glBlendFuncSeparateEXT != NULL)
	{
		glBlendFuncSeparateEXT(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_DST_ALPHA);
	//	glBlendFuncSeparateEXT(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}

	if(hasShaders)
	{
		glGenTextures (1, &oglToonTableTextureID);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_1D, oglToonTableTextureID);
		glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP); //clamp so that we dont run off the edges due to 1.0 -> [0,31] math
	}

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

	glDeleteTextures(MAX_TEXTURE, &oglTempTextureID[0]);
	glDeleteTextures(1, &oglToonTableTextureID);

	ENDGL();
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


	TexCache_SetTexture<TexFormat_32bpp>(format, texpal);
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
	if (cullingMask != 0xC0)
	{
		xglEnable(GL_CULL_FACE);
		glCullFace(map3d_cull[cullingMask>>6]);
	}
	else
		xglDisable(GL_CULL_FACE);

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

			int _envModes[4] = {0, 1, (2 + gfx3d.shading), 0};
			glUniform1i(texBlendLoc, _envModes[envMode]);
		}
	}

	xglDepthMask(enableDepthWrite?GL_TRUE:GL_FALSE);
}

static void InstallPolygonAttrib(unsigned long val)
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
	cullingMask = (val&0xC0);

	alpha31 = ((val>>16)&0x1F)==31;
	
	// Alpha value, actually not well handled, 0 should be wireframe
	wireframe = ((val>>16)&0x1F)==0;

	polyalpha = ((val>>16)&0x1F);

	// polyID
	polyID = (val>>24)&0x3F;
}

static void Control()
{
	if(gfx3d.enableTexturing) glEnable (GL_TEXTURE_2D);
	else glDisable (GL_TEXTURE_2D);

	if(gfx3d.enableAlphaTest)
		glAlphaFunc	(GL_GREATER, gfx3d.alphaTestRef/31.f);
	else
		glAlphaFunc	(GL_GREATER, 0);

	if(gfx3d.enableAlphaBlending)
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
		u16* dst = gfx3d_convertedScreen + (y<<8);
		u8* dstAlpha = gfx3d_convertedAlpha + (y<<8);

		#ifndef NOSSE2
			//I dont know much about this kind of stuff, but this seems to help
			//for some reason I couldnt make the intrinsics work 
			u8* wanx =  (u8*)&((u32*)GPU_screen3D)[i];
			#define ASS(X,Y) __asm { prefetchnta [wanx+32*0x##X##Y] }
			#define PUNK(X) ASS(X,0) ASS(X,1) ASS(X,2) ASS(X,3) ASS(X,4) ASS(X,5) ASS(X,6) ASS(X,7) ASS(X,8) ASS(X,9) ASS(X,A) ASS(X,B) ASS(X,C) ASS(X,D) ASS(X,E) ASS(X,F) 
			PUNK(0); PUNK(1);
		#endif

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
			dstAlpha[x] = alpha_5bit_to_4bit[a];
		}
	}
}


static void OGLRender()
{
	if(!BEGINGL()) return;

	Control();

	if(hasShaders)
	{
		//TODO - maybe this should only happen if the toon table is stale (for a slight speedup)

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_1D, oglToonTableTextureID);
		
		//generate a 8888 toon table from the ds format one and store it in a texture
		u32 rgbToonTable[32];
		for(int i=0;i<32;i++) rgbToonTable[i] = RGB15TO32(gfx3d.u16ToonTable[i], 255);
		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbToonTable);
	}

	xglDepthMask(GL_TRUE);

	float clearColor[4] = {
		((float)(gfx3d.clearColor&0x1F))/31.0f,
		((float)((gfx3d.clearColor>>5)&0x1F))/31.0f,
		((float)((gfx3d.clearColor>>10)&0x1F))/31.0f,
		((float)((gfx3d.clearColor>>16)&0x1F))/31.0f,
	};
	glClearColor(clearColor[0],clearColor[1],clearColor[2],clearColor[3]);
	glClearDepth(gfx3d.clearDepth);
	glClearStencil((gfx3d.clearColor >> 24) & 0x3F);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();


	//render display list
	//TODO - properly doublebuffer the display lists
	{

		u32 lastTextureFormat = 0, lastTexturePalette = 0, lastPolyAttr = 0, lastViewport = 0xFFFFFFFF;
		// int lastProjIndex = -1;

		for(int i=0;i<gfx3d.polylist->count;i++) {
			POLY *poly = &gfx3d.polylist->list[gfx3d.indexlist[i]];
			int type = poly->type;

			//a very macro-level state caching approach:
			//these are the only things which control the GPU rendering state.
			if(i==0 || lastTextureFormat != poly->texParam || lastTexturePalette != poly->texPalette || lastPolyAttr != poly->polyAttr)
			{
				isTranslucent = poly->isTranslucent();
				InstallPolygonAttrib(lastPolyAttr=poly->polyAttr);
				lastTextureFormat = textureFormat = poly->texParam;
				lastTexturePalette = texturePalette = poly->texPalette;
				lastPolyAttr = poly->polyAttr;
				BeginRenderPoly();
			}
			
			//since we havent got the whole pipeline working yet, lets use opengl for the projection
		/*	if(lastProjIndex != poly->projIndex) {
				glMatrixMode(GL_PROJECTION);
				glLoadMatrixf(gfx3d.projlist->projMatrix[poly->projIndex]);
				lastProjIndex = poly->projIndex;
			}*/

		/*	glBegin(type==3?GL_TRIANGLES:GL_QUADS);
			for(int j=0;j<type;j++) {
				VERT* vert = &gfx3d.vertlist->list[poly->vertIndexes[j]];
				u8 color[4] = {
					material_5bit_to_8bit[vert->color[0]],
					material_5bit_to_8bit[vert->color[1]],
					material_5bit_to_8bit[vert->color[2]],
					material_5bit_to_8bit[vert->color[3]]
				};
				
				//float tempCoord[4];
				//Vector4Copy(tempCoord, vert->coord);
				//we havent got the whole pipeline working yet, so we cant do this
				////convert from ds device coords to opengl
				//tempCoord[0] *= 2;
				//tempCoord[1] *= 2;
				//tempCoord[0] -= 1;
				//tempCoord[1] -= 1;

				//todo - edge flag?
				glTexCoord2fv(vert->texcoord);
				glColor4ubv((GLubyte*)color);
				//glVertex4fv(tempCoord);
				glVertex4fv(vert->coord);
			}
			glEnd();*/
		
			if(lastViewport != poly->viewport)
			{
				VIEWPORT viewport;
				viewport.decode(poly->viewport);
				glViewport(viewport.x,viewport.y,viewport.width,viewport.height);
				lastViewport = poly->viewport;
			}

			glBegin(GL_TRIANGLES);

			VERT *vert0 = &gfx3d.vertlist->list[poly->vertIndexes[0]];
			u8 alpha =	material_5bit_to_8bit[poly->getAlpha()];
			u8 color0[4] = {
					material_5bit_to_8bit[vert0->color[0]],
					material_5bit_to_8bit[vert0->color[1]],
					material_5bit_to_8bit[vert0->color[2]],
					alpha
				};

			for(int j = 1; j < (type-1); j++)
			{
				VERT *vert1 = &gfx3d.vertlist->list[poly->vertIndexes[j]];
				VERT *vert2 = &gfx3d.vertlist->list[poly->vertIndexes[j+1]];
				
				u8 color1[4] = {
					material_5bit_to_8bit[vert1->color[0]],
					material_5bit_to_8bit[vert1->color[1]],
					material_5bit_to_8bit[vert1->color[2]],
					alpha
				};
				u8 color2[4] = {
					material_5bit_to_8bit[vert2->color[0]],
					material_5bit_to_8bit[vert2->color[1]],
					material_5bit_to_8bit[vert2->color[2]],
					alpha
				};

				glTexCoord2fv(vert0->texcoord);
				glColor4ubv((GLubyte*)color0);
				glVertex4fv(vert0->coord);

				glTexCoord2fv(vert1->texcoord);
				glColor4ubv((GLubyte*)color1);
				glVertex4fv(vert1->coord);

				glTexCoord2fv(vert2->texcoord);
				glColor4ubv((GLubyte*)color2);
				glVertex4fv(vert2->coord);
			}

			glEnd();
		}
	}

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
