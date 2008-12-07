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

#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "debug.h"

//#define DEBUG_DUMP_TEXTURE

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

#ifndef CTASSERT
#define	CTASSERT(x)		typedef char __assert ## y[(x) ? 1 : -1]
#endif

static ALIGN(16) unsigned char  GPU_screen3D		[256*256*4]={0};
static ALIGN(16) unsigned char  GPU_screenStencil[256*256]={0};

static const unsigned short map3d_cull[4] = {GL_FRONT_AND_BACK, GL_FRONT, GL_BACK, 0};
static const int texEnv[4] = { GL_MODULATE, GL_DECAL, GL_MODULATE, GL_MODULATE };
static const int depthFunc[2] = { GL_LESS, GL_EQUAL };
static bool needRefreshFramebuffer = false;
static unsigned char texMAP[1024*2048*4]; 
static unsigned int textureMode=0;

float clearAlpha;


//raw ds format poly attributes, installed from the display list
static u32 textureFormat=0, texturePalette=0;

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
OGLEXT(PFNGLLINKPROGRAMPROC,glLinkProgram)
OGLEXT(PFNGLUSEPROGRAMPROC,glUseProgram)
OGLEXT(PFNGLGETSHADERIVPROC,glGetShaderiv)
OGLEXT(PFNGLGETSHADERINFOLOGPROC,glGetShaderInfoLog)
OGLEXT(PFNGLDELETESHADERPROC,glDeleteShader)
OGLEXT(PFNGLGETPROGRAMIVPROC,glGetProgramiv)
OGLEXT(PFNGLGETPROGRAMINFOLOGPROC,glGetProgramInfoLog)
OGLEXT(PFNGLVALIDATEPROGRAMPROC,glValidateProgram)
OGLEXT(PFNGLBLENDFUNCSEPARATEEXTPROC,glBlendFuncSeparateEXT)
OGLEXT(PFNGLGETUNIFORMLOCATIONPROC,glGetUniformLocation)
OGLEXT(PFNGLUNIFORM1IPROC,glUniform1i)
#endif
#ifdef _WIN32
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


//================================================= Textures
#define MAX_TEXTURE 500
struct TextureCache
{
	TextureCache()
		: suspectedInvalid(true)
	{}

	GLenum				id;
	unsigned int		frm;
	unsigned int		mode;
	unsigned int		numcolors;
	unsigned int		pal;
	unsigned int		sizeX;
	unsigned int		sizeY;
	int					coord;
	float				invSizeX;
	float				invSizeY;
	unsigned char		texture[128*1024]; // 128Kb texture slot

	//set if this texture is suspected be invalid due to a vram reconfigure
	bool				suspectedInvalid;

} ;

TextureCache	texcache[MAX_TEXTURE+1];
u32				texcache_count;

u32				texcache_start;
u32				texcache_stop;
//u32				texcache_last;

GLenum			oglTempTextureID[MAX_TEXTURE];
GLenum			oglToonTableTextureID;

#define NOSHADERS(i)					{ hasShaders = false; INFO("Shaders aren't supported on your system, using fixed pipeline\n(failed shader init at step %i)\n", i); return; }

#define SHADER_COMPCHECK(s)				{ \
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
		NOSHADERS(3); \
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
		NOSHADERS(5); \
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

/* Shaders init */

static void createShaders()
{
	hasShaders = true;

	if (glCreateShader == NULL ||  //use ==NULL instead of !func to avoid always true warnings for some systems
		glShaderSource == NULL ||
		glCompileShader == NULL ||
		glCreateProgram == NULL ||
		glAttachShader == NULL ||
		glLinkProgram == NULL ||
		glUseProgram == NULL ||
		glGetShaderInfoLog == NULL)
		NOSHADERS(1);

	vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	if(!vertexShaderID)
		NOSHADERS(2);

	glShaderSource(vertexShaderID, 1, (const GLchar**)&vertexShader, NULL);
	glCompileShader(vertexShaderID);
	SHADER_COMPCHECK(vertexShaderID);

	fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if(!fragmentShaderID)
		NOSHADERS(2);

	glShaderSource(fragmentShaderID, 1, (const GLchar**)&fragmentShader, NULL);
	glCompileShader(fragmentShaderID);
	SHADER_COMPCHECK(fragmentShaderID);

	shaderProgram = glCreateProgram();
	if(!shaderProgram)
		NOSHADERS(4);

	glAttachShader(shaderProgram, vertexShaderID);
	glAttachShader(shaderProgram, fragmentShaderID);

	glLinkProgram(shaderProgram);
	PROGRAM_COMPCHECK(shaderProgram, vertexShaderID, fragmentShaderID);

	glValidateProgram(shaderProgram);
	glUseProgram(shaderProgram);

	INFO("Successfully created OpenGL shaders.\n");
}

//=================================================


static void Reset()
{
	int i;

	//reset the texture cache
	memset(&texcache,0,sizeof(texcache));
	texcache_count=0;
	for (i = 0; i < MAX_TEXTURE; i++)
		texcache[i].id=oglTempTextureID[i];
	texcache_start=0;
	texcache_stop=MAX_TEXTURE<<1;

	//clear the framebuffers
	memset(GPU_screenStencil,0,sizeof(GPU_screenStencil));
	memset(GPU_screen3D,0,sizeof(GPU_screen3D));
}

static char Init(void)
{
	GLuint loc;

	if(!oglrender_init)
		return 0;
	if(!oglrender_init())
		return 0;

	if(!BEGINGL())
		return 0;

	glPixelStorei(GL_PACK_ALIGNMENT,8);

	xglEnable		(GL_NORMALIZE);
	xglEnable		(GL_DEPTH_TEST);
	glEnable		(GL_TEXTURE_1D);
	glEnable		(GL_TEXTURE_2D);

	glAlphaFunc		(GL_GREATER, 0);
	xglEnable		(GL_ALPHA_TEST);

	glGenTextures (MAX_TEXTURE, &oglTempTextureID[0]);

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
	INITOGLEXT(PFNGLLINKPROGRAMPROC,glLinkProgram)
	INITOGLEXT(PFNGLUSEPROGRAMPROC,glUseProgram)
	INITOGLEXT(PFNGLGETSHADERIVPROC,glGetShaderiv)
	INITOGLEXT(PFNGLGETSHADERINFOLOGPROC,glGetShaderInfoLog)
	INITOGLEXT(PFNGLDELETESHADERPROC,glDeleteShader)
	INITOGLEXT(PFNGLGETPROGRAMIVPROC,glGetProgramiv)
	INITOGLEXT(PFNGLGETPROGRAMINFOLOGPROC,glGetProgramInfoLog)
	INITOGLEXT(PFNGLVALIDATEPROGRAMPROC,glValidateProgram)
	INITOGLEXT(PFNGLBLENDFUNCSEPARATEEXTPROC,glBlendFuncSeparateEXT)
	INITOGLEXT(PFNGLGETUNIFORMLOCATIONPROC,glGetUniformLocation)
	INITOGLEXT(PFNGLUNIFORM1IPROC,glUniform1i)
#endif
#ifdef _WIN32
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
	}

	glGenTextures (1, &oglToonTableTextureID);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, oglToonTableTextureID);
	glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP); //clamp so that we dont run off the edges due to 1.0 -> [0,31] math

	if(glBlendFuncSeparateEXT == NULL)
		clearAlpha = 1;
	else 
		clearAlpha = 0;

	ENDGL();

	return 1;
}

static void Close()
{
}

//zero 9/7/08 - changed *adr= to adr= while changing from c++. was that a bug?
#define CHECKSLOT txt_slot_current_size--;\
					if (txt_slot_current_size<=0)\
					{\
						txt_slot_current++;\
						adr=(unsigned char *)ARM9Mem.textureSlotAddr[txt_slot_current];\
						txt_slot_size=txt_slot_current_size=0x020000;\
					}

//todo - make all color conversions go through a properly spread table!!

//I think this is slower than the regular memcmp.. doesnt make sense to me, but my
//asm optimization knowlege is 15 years old..

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
int memcmp_slow(const void* src, const void* dst, u32 count) {
	int retval;
	__asm {
		mov [retval], 0;
		mov ecx, [count];
		shr ecx, 2;
		mov esi, [src];
		mov edi, [dst];
		repe cmpsd;
		setc byte ptr [retval];
	}
	return retval;
}

void* memcpy_fast(void* dest, const void* src, size_t count)
{
	size_t blockCnt = count / 64;
	size_t remainder = count % 64;

	__asm
	{
		mov esi, [src] 
		mov edi, [dest] 
		mov ecx, [blockCnt]

		test ecx, ecx
		jz copy_remainder

	copyloop:
		//prefetchnta [esi]
		mov eax, [esi]
		
		movq mm0, qword ptr [esi]
		movq mm1, qword ptr [esi+8]
		movq mm2, qword ptr [esi+16]
		movq mm3, qword ptr [esi+24]
		movq mm4, qword ptr [esi+32]
		movq mm5, qword ptr [esi+40]
		movq mm6, qword ptr [esi+48]
		movq mm7, qword ptr [esi+56]
		movntq qword ptr [edi], mm0
		movntq qword ptr [edi+8], mm1
		movntq qword ptr [edi+16], mm2
		movntq qword ptr [edi+24], mm3
		movntq qword ptr [edi+32], mm4
		movntq qword ptr [edi+40], mm5
		movntq qword ptr [edi+48], mm6
		movntq qword ptr [edi+56], mm7

		add edi, 64
		add	esi, 64
		dec ecx
		jnz copyloop

		sfence
		emms

	copy_remainder:

		mov ecx, remainder
		rep movsb
	}

	return dest;
}
#else
#define memcpy_fast(d,s,c) memcpy(d,s,c)
#endif


#if defined (DEBUG_DUMP_TEXTURE) && defined (WIN32)
static void DebugDumpTexture(int which)
{
	char fname[100];
	sprintf(fname,"c:\\dump\\%d.bmp", which);

	glBindTexture(GL_TEXTURE_2D,texcache[which].id);
	  glGetTexImage( GL_TEXTURE_2D ,
			      0,
			    GL_RGBA,
			      GL_UNSIGNED_BYTE,
			      texMAP);

	NDS_WriteBMP_32bppBuffer(texcache[which].sizeX,texcache[which].sizeY,texMAP,fname);
}
#else
#define DebugDumpTexture(which) do { (void)which; } while (0)
#endif

//================================================================================
static int lastTexture = -1;
static bool hasTexture = false;
static void setTexture(unsigned int format, unsigned int texpal)
{
	int palSize[7]={32,4,16,256,0,8,32768};
	unsigned int x=0, y=0, i;
	unsigned int palZeroTransparent;
	
	u16 *pal = NULL;
	unsigned char *dst = texMAP;
	u32 *dwdst = (u32*)texMAP;
	unsigned int sizeX=(8 << ((format>>20)&0x07));
	unsigned int sizeY=(8 << ((format>>23)&0x07));
	unsigned int imageSize = sizeX*sizeY;

	u64 txt_slot_current_size;
	u64 txt_slot_size;
	u64 txt_slot_current;
	u8 *adr;

	textureMode = (unsigned short)((format>>26)&0x07);

	if (format==0)
	{
		texcache_count=-1;
		if(hasShaders && hasTexture) { glUniform1i(hasTexLoc, 0); hasTexture = false; }
		return;
	}
	if (textureMode==0)
	{
		texcache_count=-1;
		if(hasShaders && hasTexture) { glUniform1i(hasTexLoc, 0); hasTexture = false; }
		return;
	}

	if(hasShaders)
	{
		if(!hasTexture) { glUniform1i(hasTexLoc, 1); hasTexture = true; }
		glActiveTexture(GL_TEXTURE0);
	}

	txt_slot_current=(format>>14)&0x03;
	adr=(unsigned char *)(ARM9Mem.textureSlotAddr[txt_slot_current]+((format&0x3FFF)<<3));
	
	i=texcache_start;
	
	//if(false)
	while (TRUE)
	{
		if (texcache_stop==i) break;
		if (texcache[i].frm==0) break;
		if ((texcache[i].frm==format)&&(texcache[i].pal==texpal))
		{
			//TODO - we need to compare the palette also.
			//TODO - this doesnt correctly span bank boundaries. in fact, it seems quite dangerous.
			if (!texcache[i].suspectedInvalid || !memcmp(adr,texcache[i].texture,std::min((size_t)imageSize,sizeof(texcache[i].texture))))
			{
				texcache[i].suspectedInvalid = false;
				texcache_count=i;
				if(lastTexture == -1 || (int)i != lastTexture)
				{
					lastTexture = i;
					glBindTexture(GL_TEXTURE_2D,texcache[i].id);
					glMatrixMode (GL_TEXTURE);
					glLoadIdentity ();
					glScaled (texcache[i].invSizeX, texcache[i].invSizeY, 1.0f);
				}
				return;
			}
		}
		i++;
		if (i>MAX_TEXTURE) 
		{
			texcache_stop=texcache_start;
			texcache[texcache_stop].frm=0;
			texcache_start++;
			if (texcache_start>MAX_TEXTURE) 
			{
				texcache_start=0;
				texcache_stop=MAX_TEXTURE<<1;
			}
			i=0;
		}
	}

	lastTexture = i;
	glBindTexture(GL_TEXTURE_2D, texcache[i].id);

	texcache[i].suspectedInvalid = false;
	texcache[i].mode=textureMode;
	texcache[i].pal=texpal;
	texcache[i].sizeX=sizeX;
	texcache[i].sizeY=sizeY;
	texcache[i].coord=(format>>30);
	texcache[i].invSizeX=1.0f/((float)sizeX*(1<<4));
	texcache[i].invSizeY=1.0f/((float)sizeY*(1<<4));
	//memcpy(texcache[i].texture,adr,imageSize);			//======================= copy
	memcpy_fast(texcache[i].texture,adr,std::min((size_t)imageSize,sizeof(texcache[i].texture)));			//======================= copy
	texcache[i].numcolors=palSize[texcache[i].mode];

	texcache[i].frm=format;

	glMatrixMode (GL_TEXTURE);
	glLoadIdentity ();
	glScaled (texcache[i].invSizeX, texcache[i].invSizeY, 1.0f);

	//INFO("Texture %03i - format=%08X; pal=%04X (mode %X, width %04i, height %04i)\n",i, texcache[i].frm, texcache[i].pal, texcache[i].mode, sizeX, sizeY);
	
	//============================================================================ Texture render
	palZeroTransparent = (1-((format>>29)&1))*255;						// shash: CONVERT THIS TO A TABLE :)
	txt_slot_size=(txt_slot_current_size=0x020000-((format & 0x3FFF)<<3));

		switch (texcache[i].mode)
		{
			case 1: //a3i5
			{
				pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
				for(x = 0; x < imageSize; x++, dst += 4)
				{
					u16 c = pal[adr[x]&31];
					u8 alpha = adr[x]>>5;
					*dwdst++ = RGB15TO32(c,material_3bit_to_8bit[alpha]);
					CHECKSLOT;
				}
				break;
			}
			case 2: //i2
			{
				pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<3));
				for(x = 0; x < imageSize>>2; ++x)
				{
					unsigned short c = pal[(adr[x])&0x3];
					dst[0] = ((c & 0x1F)<<3);
					dst[1] = ((c & 0x3E0)>>2);
					dst[2] = ((c & 0x7C00)>>7);
					dst[3] = ((adr[x]&3) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
					dst += 4;

					c = pal[((adr[x])>>2)&0x3];
					dst[0] = ((c & 0x1F)<<3);
					dst[1] = ((c & 0x3E0)>>2);
					dst[2] = ((c & 0x7C00)>>7);
					dst[3] = (((adr[x]>>2)&3) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
					dst += 4;

					c = pal[((adr[x])>>4)&0x3];
					dst[0] = ((c & 0x1F)<<3);
					dst[1] = ((c & 0x3E0)>>2);
					dst[2] = ((c & 0x7C00)>>7);
					dst[3] = (((adr[x]>>4)&3) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
					dst += 4;

					c = pal[(adr[x])>>6];
					dst[0] = ((c & 0x1F)<<3);
					dst[1] = ((c & 0x3E0)>>2);
					dst[2] = ((c & 0x7C00)>>7);
					dst[3] = (((adr[x]>>6)&3) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
					dst += 4;
					CHECKSLOT;
				}
				break;
			}
			case 3: //i4
				{
					pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
					for(x = 0; x < (imageSize>>1); x++)
					{
						unsigned short c = pal[adr[x]&0xF];
						dst[0] = ((c & 0x1F)<<3);
						dst[1] = ((c & 0x3E0)>>2);
						dst[2] = ((c & 0x7C00)>>7);
						dst[3] = (((adr[x])&0xF) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
						dst += 4;

						c = pal[((adr[x])>>4)];
						dst[0] = ((c & 0x1F)<<3);
						dst[1] = ((c & 0x3E0)>>2);
						dst[2] = ((c & 0x7C00)>>7);
						dst[3] = (((adr[x]>>4)&0xF) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
						dst += 4;
						CHECKSLOT;
					}
					break;
				}
			case 4: //i8
				{
					pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
					for(x = 0; x < imageSize; ++x)
					{
						u16 c = pal[adr[x]];
						*dwdst++ = RGB15TO32(c,(adr[x] == 0) ? palZeroTransparent : 255);
						CHECKSLOT;
					}
				}
				break;
			case 5: //4x4
			{
				pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
				unsigned short * slot1;
				unsigned int * map = (unsigned int *)adr;
				unsigned int d = 0;
				if ( (texcache[i].frm & 0xc000) == 0x8000)
					// texel are in slot 2
					slot1=(unsigned short*)&ARM9Mem.textureSlotAddr[1][((texcache[i].frm&0x3FFF)<<2)+0x010000];
				else 
					slot1=(unsigned short*)&ARM9Mem.textureSlotAddr[1][(texcache[i].frm&0x3FFF)<<2];

				bool dead = false;

				for (y = 0; y < (texcache[i].sizeY>>2); y ++)
				{
					u32 tmpPos[4]={(y<<2)*texcache[i].sizeX,((y<<2)+1)*texcache[i].sizeX,
												((y<<2)+2)*texcache[i].sizeX,((y<<2)+3)*texcache[i].sizeX};
					for (x = 0; x < (texcache[i].sizeX>>2); x ++, d++)
						{
						u32 currBlock	= map[d], sy;
						u16 pal1		= slot1[d];
						u16 pal1offset	= (pal1 & 0x3FFF)<<1;
						u8  mode		= pal1>>14;
						u32 tmp_col[4];

						tmp_col[0]=RGB16TO32(pal[pal1offset],255);
						tmp_col[1]=RGB16TO32(pal[pal1offset+1],255);

						switch (mode) 
						{
							case 0:
								tmp_col[2]=RGB16TO32(pal[pal1offset+2],255);
								tmp_col[3]=RGB16TO32(0x7FFF,0);
								break;
							case 1:
								tmp_col[2]=(((tmp_col[0]&0xFF)+(tmp_col[1]&0xff))>>1)|
												(((tmp_col[0]&(0xFF<<8))+(tmp_col[1]&(0xFF<<8)))>>1)|
													(((tmp_col[0]&(0xFF<<16))+(tmp_col[1]&(0xFF<<16)))>>1)|
														(0xff<<24);
								tmp_col[3]=RGB16TO32(0x7FFF,0);
								break;
							case 2:
								tmp_col[2]=RGB16TO32(pal[pal1offset+2],255);
								tmp_col[3]=RGB16TO32(pal[pal1offset+3],255);
								break;
							case 3: 
							{
								u32 red1, red2;
								u32 green1, green2;
								u32 blue1, blue2;
								u16 tmp1, tmp2;

								red1=tmp_col[0]&0xff;
								green1=(tmp_col[0]>>8)&0xff;
								blue1=(tmp_col[0]>>16)&0xff;
								red2=tmp_col[1]&0xff;
								green2=(tmp_col[1]>>8)&0xff;
								blue2=(tmp_col[1]>>16)&0xff;

								tmp1=((red1*5+red2*3)>>6)|
											(((green1*5+green2*3)>>6)<<5)|
												(((blue1*5+blue2*3)>>6)<<10);
								tmp2=((red2*5+red1*3)>>6)|
											(((green2*5+green1*3)>>6)<<5)|
											  (((blue2*5+blue1*3)>>6)<<10);

								tmp_col[2]=RGB16TO32(tmp1,255);
								tmp_col[3]=RGB16TO32(tmp2,255);
								break;
							}
						  }
							for (sy = 0; sy < 4; sy++)
							{
								// Texture offset
								u32 currentPos = (x<<2) + tmpPos[sy];
								u8 currRow = (u8)((currBlock>>(sy<<3))&0xFF);

								dwdst[currentPos] = tmp_col[currRow&3];
								dwdst[currentPos+1] = tmp_col[(currRow>>2)&3];
								dwdst[currentPos+2] = tmp_col[(currRow>>4)&3];
								dwdst[currentPos+3] = tmp_col[(currRow>>6)&3];

								if(dead) {
									memset(dwdst, 0, sizeof(dwdst[0]) * 4);
								}

								txt_slot_current_size-=4;;
								if (txt_slot_current_size<=0)
								{
									//dead = true;
									txt_slot_current++;
									map=(unsigned int*)ARM9Mem.textureSlotAddr[txt_slot_current];
									map-=txt_slot_size>>2; //this is weird, but necessary since we use map[d] above
									txt_slot_size=txt_slot_current_size=0x020000;
								}
							}
						}
					}


				break;
			}
			case 6: //a5i3
			{
				pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
				for(x = 0; x < imageSize; x++)
				{
					u16 c = pal[adr[x]&0x07];
					u8 alpha = (adr[x]>>3);
					*dwdst++ = RGB15TO32(c,material_5bit_to_8bit[alpha]);
					CHECKSLOT;
				}
				break;
			}
			case 7: //16bpp
			{
				unsigned short * map = ((unsigned short *)adr);
				pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
				
				for(x = 0; x < imageSize; ++x)
				{
					u16 c = map[x];
					int alpha = ((c&0x8000)?255:0);
					*dwdst++ = RGB15TO32(c&0x7FFF,alpha);
					
					txt_slot_current_size-=2;;
					if (txt_slot_current_size<=0)
					{
						txt_slot_current++;
						map=(unsigned short *)ARM9Mem.textureSlotAddr[txt_slot_current];
						map-=txt_slot_size>>1;
						txt_slot_size=txt_slot_current_size=0x020000;
					}
				}
				break;
			}
	}


	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
						texcache[i].sizeX, texcache[i].sizeY, 0, 
							GL_RGBA, GL_UNSIGNED_BYTE, texMAP);

	DebugDumpTexture(i);

	//============================================================================================

	texcache_count=i;
	
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (BIT16(texcache[i].frm) ? (BIT18(texcache[i].frm)?GL_MIRRORED_REPEAT:GL_REPEAT) : GL_CLAMP));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (BIT17(texcache[i].frm) ? (BIT19(texcache[i].frm)?GL_MIRRORED_REPEAT:GL_REPEAT) : GL_CLAMP));
}



//controls states:
//glStencilFunc
//glStencilOp
//glColorMask
static u32 stencilStateSet = -1;

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
			enableDepthWrite = true;
			if(stencilStateSet!=0) {
				stencilStateSet = 0;
				//when the polyID is zero, we are writing the shadow mask.
				//set stencilbuf = 1 where the shadow volume is obstructed by geometry.
				//do not write color or depth information.
				glStencilFunc(GL_ALWAYS,2,255);
				glStencilOp(GL_KEEP,GL_REPLACE,GL_KEEP);
				glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
			}
		} else {
			enableDepthWrite = false;
			if(stencilStateSet!=1) {
				stencilStateSet = 1;
				//when the polyid is nonzero, we are drawing the shadow poly.
				//only draw the shadow poly where the stencilbuf==1.
				//I am not sure whether to update the depth buffer here--so I chose not to.
				glStencilFunc(GL_EQUAL,2,255);
				glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
				glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
			}
		}
	} else {
		xglEnable(GL_STENCIL_TEST);
		if(stencilStateSet!=2) {
			stencilStateSet=2;
			glStencilFunc(GL_ALWAYS,1,255);
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

	// polyID
	polyID = (val>>24)&0x1F;
}

static void Control()
{
	if(gfx3d.enableTexturing) glEnable (GL_TEXTURE_2D);
	else glDisable (GL_TEXTURE_2D);

	if(gfx3d.enableAlphaTest)
		glAlphaFunc	(GL_GREATER, gfx3d.alphaTestRef);
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


static void Render()
{
	if(!BEGINGL()) return;

	Control();

	if(hasShaders)
	{
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_1D, oglToonTableTextureID);
		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, gfx3d.rgbToonTable);
	}

	xglDepthMask(GL_TRUE);

	glViewport(gfx3d.viewport.x,gfx3d.viewport.y,gfx3d.viewport.width,gfx3d.viewport.height);
	
	//we're not using the alpha clear color right now
	glClearColor(gfx3d.clearColor[0],gfx3d.clearColor[1],gfx3d.clearColor[2], clearAlpha);
	glClearDepth(gfx3d.clearDepth);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);


	//render display list
	//TODO - properly doublebuffer the display lists
	{

		u32 lastTextureFormat = 0, lastTexturePalette = 0, lastPolyAttr = 0;

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
				BeginRenderPoly();
			}
			
			//since we havent got the whole pipeline working yet, lets use opengl for the projection
			glMatrixMode(GL_PROJECTION);
			glLoadMatrixf(poly->projMatrix);

			glBegin(type==3?GL_TRIANGLES:GL_QUADS);
			for(int j=0;j<type;j++) {
				VERT* vert = &gfx3d.vertlist->list[poly->vertIndexes[j]];
				u8 color[4] = {
					material_5bit_to_8bit[vert->color[0]],
					material_5bit_to_8bit[vert->color[1]],
					material_5bit_to_8bit[vert->color[2]],
					material_5bit_to_8bit[vert->color[3]]
				};
				
				//float tempCoord[4];
				//Vector4Copy(tempCoord,vert->coord);
				//we havent got the whole pipeline working yet, so we cant do this
				////convert from ds device coords to opengl
				//tempCoord[0] *= 2;
				//tempCoord[1] *= 2;
				//tempCoord[0] -= 1;
				//tempCoord[1] -= 1;

				//todo - edge flag?
				glTexCoord2fv(vert->texcoord);
				glColor4ubv((GLubyte*)color);
				//glVertex3fv(tempCoord);
				glVertex3fv(vert->coord);
			}
			glEnd();
		}
	}

	//since we just redrew, we need to refresh the framebuffers
	needRefreshFramebuffer = true;

	ENDGL();
}

static void VramReconfigureSignal()
{
	//well, this is a very blunt instrument.
	//lets just flag all the textures as invalid.
	for(int i=0;i<MAX_TEXTURE+1;i++)
		texcache[i].suspectedInvalid = true;
}

static void GL_ReadFramebuffer()
{
	if(!BEGINGL()) return; 
	glFinish();
	glReadPixels(0,0,256,192,GL_RGBA,				GL_UNSIGNED_BYTE,	GPU_screen3D);	
	glReadPixels(0,0,256,192,GL_STENCIL_INDEX,		GL_UNSIGNED_BYTE,	GPU_screenStencil);
	ENDGL();

//debug: view depth buffer via color buffer for debugging
	//int ctr=0;
	//for(ctr=0;ctr<256*192;ctr++) {
	//	float zval = GPU_screen3Ddepth[ctr];
	//	u8* colorPtr = GPU_screen3D+ctr*3;
	//	if(zval<0) {
	//		colorPtr[0] = 255;
	//		colorPtr[1] = 0;
	//		colorPtr[2] = 0;
	//	} else if(zval>1) {
	//		colorPtr[0] = 0;
	//		colorPtr[1] = 0;
	//		colorPtr[2] = 255;
	//	} else {
	//		colorPtr[0] = colorPtr[1] = colorPtr[2] = zval*255;
	//		//INFO("%f %f %d\n",zval, zval*255,colorPtr[0]);
	//	}

	//}
}

//NHerve mod3 - Fixed blending with 2D backgrounds (New Super Mario Bros looks better)
//zeromus post-mod3: fix even better
static void GetLine (int line, u16* dst)
{
	assert(line<192 && line>=0);

	if(needRefreshFramebuffer) {
		needRefreshFramebuffer = false;
		GL_ReadFramebuffer();
	}
	
	u8 *screen3D = (u8*)GPU_screen3D+((191-line)<<10);
	u8 *screenStencil = (u8*)GPU_screenStencil+((191-line)<<8);

	//the renderer clears the stencil to 0
	//then it sets it to 1 whenever it renders a pixel that passes the alpha test
	//(it also sets it to 2 under some circumstances when rendering shadow volumes)
	//so, we COULD use a zero stencil value to indicate that nothing should get composited.
	//in fact, we are going to do that to fix some problems. 
	//but beware that it i figure it might could CAUSE some problems

	//this alpha compositing blending logic isnt thought through at all
	//someone needs to think about what bitdepth it should take place at and how to do it efficiently

	for(int i = 0; i < 256; i++)
	{
		u32 stencil = screenStencil[i];

		//you would use this if you wanted to use the stencil buffer to make decisions here
		if(!stencil) continue;

		int t=i<<2;
		u32 r = screen3D[t+0];
		u32 g = screen3D[t+1];
		u32 b = screen3D[t+2];
		u32 a = screen3D[t+3];

		u32 oldcolor = RGB15TO32(dst[i],0);
		u32 oldr = oldcolor&0xFF;
		u32 oldg = (oldcolor>>8)&0xFF;
		u32 oldb = (oldcolor>>16)&0xFF;

		r = (r*a + oldr*(255-a)) >> 8;
		g = (g*a + oldg*(255-a)) >> 8;
		b = (b*a + oldb*(255-a)) >> 8;

		r=std::min((u32)255,r);
		g=std::min((u32)255,g);
		b=std::min((u32)255,b);

		//debug: display alpha channel
		//u32 r = screen3D[t+3];
		//u32 g = screen3D[t+3];
		//u32 b = screen3D[t+3];

		dst[i] = ((b>>3)<<10) | ((g>>3)<<5) | (r>>3);
	}
}





GPU3DInterface gpu3Dgl = {	
	Init,
	Reset,
	Close,
	Render,
	VramReconfigureSignal,
	GetLine
};



