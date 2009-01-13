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

//This class represents a number of regions of memory which should be viewed as contiguous
class MemSpan
{
public:
	static const int MAXSIZE = 8;

	MemSpan() 
		: numItems(0)
	{}

	int numItems;

	struct Item {
		u32 start;
		u32 len;
		u8* ptr;
		u32 ofs; //offset within the memspan
	} items[MAXSIZE];

	int size;

	//this MemSpan shall be considered the first argument to a standard memcmp
	//the length shall be as specified in this MemSpan, unless you specify otherwise
	int memcmp(void* buf2, int size=-1)
	{
		if(size==-1) size = this->size;
		size = std::min(this->size,size);
		for(int i=0;i<numItems;i++)
		{
			Item &item = items[i];
			int todo = std::min((int)item.len,size);
			size -= todo;
			int temp = ::memcmp(item.ptr,((u8*)buf2)+item.ofs,todo);
			if(temp) return temp;
			if(size == 0) break;
		}
		return 0;
	}

	//dumps the memspan to the specified buffer
	//you may set size to limit the size to be copied
	int dump(void* buf, int size=-1)
	{
		if(size==-1) size = this->size;
		size = std::min(this->size,size);
		u8* bufptr = (u8*)buf;
		int done = 0;
		for(int i=0;i<numItems;i++)
		{
			Item item = items[i];
			int todo = std::min((int)item.len,size);
			size -= todo;
			done += todo;
			memcpy(bufptr,item.ptr,todo);
			bufptr += todo;
			if(size==0) return done;
		}
		return done;
	}
};

//creates a MemSpan in texture memory
static MemSpan MemSpan_TexMem(u32 ofs, u32 len) 
{
	MemSpan ret;
	ret.size = len;
	u32 currofs = 0;
	while(len) {
		MemSpan::Item &curr = ret.items[ret.numItems++];
		curr.start = ofs&0x1FFFF;
		u32 slot = (ofs>>17)&3; //slots will wrap around
		curr.len = std::min(len,0x20000-curr.start);
		curr.ofs = currofs;
		len -= curr.len;
		ofs += curr.len;
		currofs += curr.len;
		u8* ptr = ARM9Mem.textureSlotAddr[slot];
		//this is just a guess. what happens if there is a gap in the mapping? lets put zeros
		if(ptr == NULL) {
			PROGINFO("Texture gap in memory mapping. Trying to accomodate.\n");
			static u8* emptyTextureSlot = 0;
			if(emptyTextureSlot == NULL) {
				emptyTextureSlot = new u8[128*1024];
				memset(emptyTextureSlot,0,128*1024);
			}
			ptr = emptyTextureSlot;
		}

		curr.ptr = ptr + curr.start;
	}
	return ret;
}

//creates a MemSpan in texture palette memory
static MemSpan MemSpan_TexPalette(u32 ofs, u32 len) 
{
	MemSpan ret;
	ret.size = len;
	u32 currofs = 0;
	while(len) {
		MemSpan::Item &curr = ret.items[ret.numItems++];
		curr.start = ofs&0x3FFF;
		u32 slot = (ofs>>14)&7; //this masks to 8 slots, but there are really only 6
		if(slot>5) {
			PROGINFO("Texture palette overruns texture memory. Wrapping at palette slot 0.\n");
			slot -= 5;
		}
		curr.len = std::min(len,0x4000-curr.start);
		curr.ofs = currofs;
		len -= curr.len;
		ofs += curr.len;
		//if(len != 0) 
			//here is an actual test case of bank spanning
		currofs += curr.len;
		u8* ptr = ARM9Mem.texPalSlot[slot];
		//this is just a guess. what happens if there is a gap in the mapping? lets put zeros
		if(ptr == NULL) {
			PROGINFO("Texture palette gap in memory mapping. Trying to accomodate.\n");
			static u8* emptyTexturePalette = 0;
			if(emptyTexturePalette == NULL) {
				emptyTexturePalette = new u8[16*1024];
				memset(emptyTexturePalette,0,16*1024);
			}
			ptr = emptyTexturePalette;
		}
		curr.ptr = ptr + curr.start;
	}
	return ret;
}


#ifndef CTASSERT
#define	CTASSERT(x)		typedef char __assert ## y[(x) ? 1 : -1]
#endif

static ALIGN(16) u8  GPU_screen3D		[256*192*4];
//static ALIGN(16) unsigned char  GPU_screenStencil[256*256];

static const unsigned short map3d_cull[4] = {GL_FRONT_AND_BACK, GL_FRONT, GL_BACK, 0};
static const int texEnv[4] = { GL_MODULATE, GL_DECAL, GL_MODULATE, GL_MODULATE };
static const int depthFunc[2] = { GL_LESS, GL_EQUAL };
static bool needRefreshFramebuffer = false;
static unsigned char texMAP[1024*2048*4]; 
static unsigned int textureMode=TEXMODE_NONE;

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


//================================================= Textures
#define MAX_TEXTURE 500
#ifdef SSE2
struct ALIGN(16) TextureCache
#else
struct ALIGN(8) TextureCache
#endif
{
	GLenum				id;
	u32					frm;
	u32					mode;
	u32					pal;
	u32					sizeX;
	u32					sizeY;
	float				invSizeX;
	float				invSizeY;
	int					textureSize, indexSize;
	u8					texture[128*1024]; // 128Kb texture slot
	u8					palette[256*2];

	//set if this texture is suspected be invalid due to a vram reconfigure
	bool				suspectedInvalid;

};

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

#ifdef HAVE_LIBOSMESA
	NOSHADERS(1);
#endif

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


static void OGLReset()
{
	int i;

	//reset the texture cache
	memset(&texcache,0,sizeof(texcache));
	texcache_count=0;
	for (i = 0; i < MAX_TEXTURE; i++)
		texcache[i].id=oglTempTextureID[i];
	texcache_start=0;
	texcache_stop=MAX_TEXTURE<<1;

	for(i=0;i<MAX_TEXTURE+1;i++)
		texcache[i].suspectedInvalid = true;

	//clear the framebuffers
//	memset(GPU_screenStencil,0,sizeof(GPU_screenStencil));
	memset(GPU_screen3D,0,sizeof(GPU_screen3D));

}

static char OGLInit(void)
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
		glUniform1i(hasTexLoc, 1);

		texBlendLoc = glGetUniformLocation(shaderProgram, "texBlending");
		glUniform1i(texBlendLoc, 0);
	}

	//we want to use alpha destination blending so we can track the last-rendered alpha value
	if(glBlendFuncSeparateEXT != NULL)
	{
		glBlendFuncSeparateEXT(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_DST_ALPHA);
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

	if(glBlendFuncSeparateEXT == NULL)
		clearAlpha = 1;
	else 
		clearAlpha = 0;

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

//todo - make all color conversions go through a properly spread table!!

#if defined (DEBUG_DUMP_TEXTURE) && defined (WIN32)
static void DebugDumpTexture(int which)
{
	char fname[100];
	sprintf(fname,"c:\\dump\\%d.bmp", which);

	glBindTexture(GL_TEXTURE_2D,texcache[which].id);
	  glGetTexImage( GL_TEXTURE_2D ,
			      0,
			    GL_BGRA_EXT,
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
	//for each texformat, number of palette entries
	const int palSizes[] = {0, 32, 4, 16, 256, 0, 8, 0};

	//for each texformat, multiplier from numtexels to numbytes (fixed point 30.2)
	const int texSizes[] = {0, 4, 1, 2, 4, 1, 4, 8};

	//used to hold a copy of the palette specified for this texture
	u16 pal[256];

	u32 *dwdst = (u32*)texMAP;
	
	textureMode = (unsigned short)((format>>26)&0x07);
	unsigned int sizeX=(8 << ((format>>20)&0x07));
	unsigned int sizeY=(8 << ((format>>23)&0x07));
	unsigned int imageSize = sizeX*sizeY;

	u8 *adr;


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

	u32 paletteAddress;

	switch (textureMode)
	{
	case TEXMODE_I2:
		paletteAddress = texturePalette<<3;
		break;
	case TEXMODE_A3I5: //a3i5
	case TEXMODE_I4: //i4
	case TEXMODE_I8: //i8
	case TEXMODE_A5I3: //a5i3
	case TEXMODE_16BPP: //16bpp
	case TEXMODE_4X4: //4x4
	default:
		paletteAddress = texturePalette<<4;
		break;
	}

	//analyze the texture memory mapping and the specifications of this texture
	int palSize = palSizes[textureMode];
	int texSize = (imageSize*texSizes[textureMode])>>2; //shifted because the texSizes multiplier is fixed point
	MemSpan ms = MemSpan_TexMem((format&0xFFFF)<<3,texSize);
	MemSpan mspal = MemSpan_TexPalette(paletteAddress,palSize*2);

	//determine the location for 4x4 index data
	u32 indexBase;
	if((format & 0xc000) == 0x8000) indexBase = 0x30000;
	else indexBase = 0x20000;

	u32 indexOffset = (format&0x3FFF)<<2;

	int indexSize = 0;
	MemSpan msIndex;
	if(textureMode == TEXMODE_4X4)
	{
		indexSize = imageSize>>3;
		msIndex = MemSpan_TexMem(indexOffset+indexBase,indexSize);
	}


	//dump the palette to a temp buffer, so that we don't have to worry about memory mapping.
	//this isnt such a problem with texture memory, because we read sequentially from it.
	//however, we read randomly from palette memory, so the mapping is more costly.
	mspal.dump(pal);


	u32 tx=texcache_start;

	//if(false)
	while (TRUE)
	{
		//conditions where we give up and regenerate the texture:
		if (texcache_stop == tx) break;
		if (texcache[tx].frm == 0) break;

		//conditions where we reject matches:
		//when the teximage or texpal params dont match 
		//(this is our key for identifying palettes in the cache)
		if (texcache[tx].frm != format) goto REJECT;
		if (texcache[tx].pal != texpal) goto REJECT;

		//the texture matches params, but isnt suspected invalid. accept it.
		if (!texcache[tx].suspectedInvalid) goto ACCEPT;

		//if we couldnt cache this entire texture due to it being too large, then reject it
		if (texSize+indexSize > (int)sizeof(texcache[tx].texture)) goto REJECT;

		//when the palettes dont match:
		//note that we are considering 4x4 textures to have a palette size of 0.
		//they really have a potentially HUGE palette, too big for us to handle like a normal palette,
		//so they go through a different system
		if (mspal.size != 0 && memcmp(texcache[tx].palette,pal,mspal.size)) goto REJECT;

		//when the texture data doesn't match
		if(ms.memcmp(texcache[tx].texture,sizeof(texcache[tx].texture))) goto REJECT;

		//if the texture is 4x4 then the index data must match
		if(textureMode == TEXMODE_4X4)
		{
			if(msIndex.memcmp(texcache[tx].texture + texcache[tx].textureSize,texcache[tx].indexSize)) goto REJECT; 
		}


ACCEPT:
		texcache[tx].suspectedInvalid = false;
		texcache_count = tx;
		if(lastTexture == -1 || (int)tx != lastTexture)
		{
			lastTexture = tx;
			glBindTexture(GL_TEXTURE_2D,texcache[tx].id);
			glMatrixMode (GL_TEXTURE);
			glLoadIdentity ();
			glScaled (texcache[tx].invSizeX, texcache[tx].invSizeY, 1.0f);
		}
		return;
 
REJECT:
		tx++;
		if ( tx > MAX_TEXTURE )
		{
			texcache_stop=texcache_start;
			texcache[texcache_stop].frm=0;
			texcache_start++;
			if (texcache_start>MAX_TEXTURE) 
			{
				texcache_start=0;
				texcache_stop=MAX_TEXTURE<<1;
			}
			tx=0;
		}
	}

	lastTexture = tx;
	glBindTexture(GL_TEXTURE_2D, texcache[tx].id);

	texcache[tx].suspectedInvalid = false;
	texcache[tx].frm=format;
	texcache[tx].mode=textureMode;
	texcache[tx].pal=texpal;
	texcache[tx].sizeX=sizeX;
	texcache[tx].sizeY=sizeY;
	texcache[tx].invSizeX=1.0f/((float)(sizeX));
	texcache[tx].invSizeY=1.0f/((float)(sizeY));
	texcache[tx].textureSize = ms.dump(texcache[tx].texture,sizeof(texcache[tx].texture));

	//dump palette data for cache keying
	if ( palSize )
	{
		memcpy(texcache[tx].palette, pal, palSize*2);
	}
	//dump 4x4 index data for cache keying
	texcache[tx].indexSize = 0;
	if(textureMode == TEXMODE_4X4)
	{
		texcache[tx].indexSize = std::min(msIndex.size,(int)sizeof(texcache[tx].texture) - texcache[tx].textureSize);
		msIndex.dump(texcache[tx].texture+texcache[tx].textureSize,texcache[tx].indexSize);
	}


	glMatrixMode (GL_TEXTURE);
	glLoadIdentity ();
	glScaled (texcache[tx].invSizeX, texcache[tx].invSizeY, 1.0f);


	//INFO("Texture %03i - format=%08X; pal=%04X (mode %X, width %04i, height %04i)\n",i, texcache[i].frm, texcache[i].pal, texcache[i].mode, sizeX, sizeY);

	//============================================================================ Texture conversion
	u32 palZeroTransparent = (1-((format>>29)&1))*255;						// shash: CONVERT THIS TO A TABLE :)

	switch (texcache[tx].mode)
	{
	case TEXMODE_A3I5:
		{
			for(int j=0;j<ms.numItems;j++) {
				adr = ms.items[j].ptr;
				for(u32 x = 0; x < ms.items[j].len; x++)
				{
					u16 c = pal[*adr&31];
					u8 alpha = *adr>>5;
					*dwdst++ = RGB15TO32(c,material_3bit_to_8bit[alpha]);
					adr++;
				}
			}

			break;
		}
	case TEXMODE_I2:
		{
			for(int j=0;j<ms.numItems;j++) {
				adr = ms.items[j].ptr;
				for(u32 x = 0; x < ms.items[j].len; x++)
				{
					u8 bits;
					u16 c;

					bits = (*adr)&0x3;
					c = pal[bits];
					*dwdst++ = RGB15TO32(c,(bits == 0) ? palZeroTransparent : 255);

					bits = ((*adr)>>2)&0x3;
					c = pal[bits];
					*dwdst++ = RGB15TO32(c,(bits == 0) ? palZeroTransparent : 255);

					bits = ((*adr)>>4)&0x3;
					c = pal[bits];
					*dwdst++ = RGB15TO32(c,(bits == 0) ? palZeroTransparent : 255);

					bits = ((*adr)>>6)&0x3;
					c = pal[bits];
					*dwdst++ = RGB15TO32(c,(bits == 0) ? palZeroTransparent : 255);

					adr++;
				}
			}
			break;
		}
	case TEXMODE_I4:
		{
			for(int j=0;j<ms.numItems;j++) {
				adr = ms.items[j].ptr;
				for(u32 x = 0; x < ms.items[j].len; x++)
				{
					u8 bits;
					u16 c;

					bits = (*adr)&0xF;
					c = pal[bits];
					*dwdst++ = RGB15TO32(c,(bits == 0) ? palZeroTransparent : 255);

					bits = ((*adr)>>4);
					c = pal[bits];
					*dwdst++ = RGB15TO32(c,(bits == 0) ? palZeroTransparent : 255);

					adr++;
				}
			}
			break;
		}
	case TEXMODE_I8:
		{
			for(int j=0;j<ms.numItems;j++) {
				adr = ms.items[j].ptr;
				for(u32 x = 0; x < ms.items[j].len; ++x)
				{
					u16 c = pal[*adr];
					*dwdst++ = RGB15TO32(c,(*adr == 0) ? palZeroTransparent : 255);
					adr++;
				}
			}
		}
		break;
	case TEXMODE_4X4:
		{
			//RGB16TO32 is used here because the other conversion macros result in broken interpolation logic

			if(ms.numItems != 1) {
				PROGINFO("Your 4x4 texture has overrun its texture slot.\n");
			}
			//this check isnt necessary since the addressing is tied to the texture data which will also run out:
			//if(msIndex.numItems != 1) PROGINFO("Your 4x4 texture index has overrun its slot.\n");

#define PAL4X4(offset) ( *(u16*)( ARM9Mem.texPalSlot[((paletteAddress + (offset)*2)>>14)] + ((paletteAddress + (offset)*2)&0x3FFF) ) )

			u16* slot1;
			u32* map = (u32*)ms.items[0].ptr;
			u32 limit = ms.items[0].len<<2;
			u32 d = 0;
			if ( (texcache[tx].frm & 0xc000) == 0x8000)
				// texel are in slot 2
				slot1=(u16*)&ARM9Mem.textureSlotAddr[1][((texcache[tx].frm & 0x3FFF)<<2)+0x010000];
			else 
				slot1=(u16*)&ARM9Mem.textureSlotAddr[1][(texcache[tx].frm & 0x3FFF)<<2];

			u16 yTmpSize = (texcache[tx].sizeY>>2);
			u16 xTmpSize = (texcache[tx].sizeX>>2);

			//this is flagged whenever a 4x4 overruns its slot.
			//i am guessing we just generate black in that case
			bool dead = false;

			for (int y = 0; y < yTmpSize; y ++)
			{
				u32 tmpPos[4]={(y<<2)*texcache[tx].sizeX,((y<<2)+1)*texcache[tx].sizeX,
					((y<<2)+2)*texcache[tx].sizeX,((y<<2)+3)*texcache[tx].sizeX};
				for (int x = 0; x < xTmpSize; x ++, d++)
				{
					if(d >= limit)
						dead = true;

					if(dead) {
						for (int sy = 0; sy < 4; sy++)
						{
							u32 currentPos = (x<<2) + tmpPos[sy];
							dwdst[currentPos] = dwdst[currentPos+1] = dwdst[currentPos+2] = dwdst[currentPos+3] = 0;
						}
						continue;
					}

					u32 currBlock	= map[d];
					u16 pal1		= slot1[d];
					u16 pal1offset	= (pal1 & 0x3FFF)<<1;
					u8  mode		= pal1>>14;
					u32 tmp_col[4];

					tmp_col[0]=RGB16TO32(PAL4X4(pal1offset),255);
					tmp_col[1]=RGB16TO32(PAL4X4(pal1offset+1),255);

					switch (mode) 
					{
					case 0:
						tmp_col[2]=RGB16TO32(PAL4X4(pal1offset+2),255);
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
						tmp_col[2]=RGB16TO32(PAL4X4(pal1offset+2),255);
						tmp_col[3]=RGB16TO32(PAL4X4(pal1offset+3),255);
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

					//set all 16 texels
					for (int sy = 0; sy < 4; sy++)
					{
						// Texture offset
						u32 currentPos = (x<<2) + tmpPos[sy];
						u8 currRow = (u8)((currBlock>>(sy<<3))&0xFF);

						dwdst[currentPos] = tmp_col[currRow&3];
						dwdst[currentPos+1] = tmp_col[(currRow>>2)&3];
						dwdst[currentPos+2] = tmp_col[(currRow>>4)&3];
						dwdst[currentPos+3] = tmp_col[(currRow>>6)&3];
					}


				}
			}


			break;
		}
	case TEXMODE_A5I3:
		{
			for(int j=0;j<ms.numItems;j++) {
				adr = ms.items[j].ptr;
				for(u32 x = 0; x < ms.items[j].len; ++x)
				{
					u16 c = pal[*adr&0x07];
					u8 alpha = (*adr>>3);
					*dwdst++ = RGB15TO32(c,material_5bit_to_8bit[alpha]);
					adr++;
				}
			}
			break;
		}
	case TEXMODE_16BPP:
		{
			for(int j=0;j<ms.numItems;j++) {
				u16* map = (u16*)ms.items[j].ptr;
				for(u32 x = 0; x < ms.items[j].len; ++x)
				{
					u16 c = map[x];
					int alpha = ((c&0x8000)?255:0);
					*dwdst++ = RGB15TO32(c&0x7FFF,alpha);
				}
			}
			break;
		}
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
		texcache[tx].sizeX, texcache[tx].sizeY, 0, 
		GL_RGBA, GL_UNSIGNED_BYTE, texMAP);

	DebugDumpTexture(tx);

	//============================================================================================

	texcache_count=tx;

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (BIT16(texcache[tx].frm) ? (BIT18(texcache[tx].frm)?GL_MIRRORED_REPEAT:GL_REPEAT) : GL_CLAMP));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (BIT17(texcache[tx].frm) ? (BIT19(texcache[tx].frm)?GL_MIRRORED_REPEAT:GL_REPEAT) : GL_CLAMP));
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
			enableDepthWrite = false;
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
			enableDepthWrite = true;
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

static void OGLRender()
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

	//it might be handy to print the size of the projection list, in case a game is doing something weird with it
	//printf("%d\n",gfx3d.projlist->count);
	
	//we're not using the alpha clear color right now
	glClearColor(gfx3d.clearColor[0],gfx3d.clearColor[1],gfx3d.clearColor[2], gfx3d.clearColor[3]);
	glClearDepth(gfx3d.clearDepth);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);


	//render display list
	//TODO - properly doublebuffer the display lists
	{

		u32 lastTextureFormat = 0, lastTexturePalette = 0, lastPolyAttr = 0;
		int lastProjIndex = -1;

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
			if(lastProjIndex != poly->projIndex) {
				glMatrixMode(GL_PROJECTION);
				glLoadMatrixf(gfx3d.projlist->projMatrix[poly->projIndex]);
				lastProjIndex = poly->projIndex;
			}

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

static void OGLVramReconfigureSignal()
{
	//well, this is a very blunt instrument.
	//lets just flag all the textures as invalid.
	for(int i=0;i<MAX_TEXTURE+1;i++) {
		texcache[i].suspectedInvalid = true;

		//invalidate all 4x4 textures when texture palettes change mappings
		//this is necessary because we arent tracking 4x4 texture palettes to look for changes.
		//Although I concede this is a bit paranoid.. I think the odds of anyone changing 4x4 palette data
		//without also changing the texture data is pretty much zero.
		//
		//TODO - move this to a separate signal: split into TexReconfigureSignal and TexPaletteReconfigureSignal
		if(texcache[i].mode == TEXMODE_4X4)
			texcache[i].frm = 0;
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
	for(int i=0;i<256*192;i++) {
		u32 &u32screen3D = ((u32*)GPU_screen3D)[i];
		u32screen3D>>=3;
		u32screen3D &= 0x1F1F1F1F;
	}
		

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

static void OGLGetLineCaptured(int line, u16* dst)
{
	if(needRefreshFramebuffer) {
		needRefreshFramebuffer = false;
		GL_ReadFramebuffer();
	}

	u8 *screen3D = (u8*)GPU_screen3D+((191-line)<<10);
//	u8 *screenStencil = (u8*)GPU_screenStencil+((191-line)<<8);

	for(int i = 0; i < 256; i++)
	{
	/*	u32 stencil = screenStencil[i];

		if(!stencil) 
		{
			dst[i] = 0x0000;
			continue;
		}*/

		int t=i<<2;
	/*	u8 r = screen3D[t+2];
		u8 g = screen3D[t+1];
		u8 b = screen3D[t+0];*/

		//if this math strikes you as wrong, be sure to look at GL_ReadFramebuffer() where the pixel format in screen3D is changed
		//dst[i] = (b<<10) | (g<<5) | (r) | 0x8000;
		dst[i] = (screen3D[t+2] | (screen3D[t+1] << 5) | (screen3D[t+0] << 10) | ((screen3D[t+3] > 0) ? 0x8000 : 0x0000));
	}
}


static void OGLGetLine(int line, int start, int end_inclusive, u16* dst, u8* dstAlpha)
{
	assert(line<192 && line>=0);

	if(needRefreshFramebuffer) {
		needRefreshFramebuffer = false;
		GL_ReadFramebuffer();
	}

	u8 *screen3D = (u8*)GPU_screen3D+((191-line)<<10);
	//u8 *screenStencil = (u8*)GPU_screenStencil+((191-line)<<8);

	//the renderer clears the stencil to 0
	//then it sets it to 1 whenever it renders a pixel that passes the alpha test
	//(it also sets it to 2 under some circumstances when rendering shadow volumes)
	//so, we COULD use a zero stencil value to indicate that nothing should get composited.
	//in fact, we are going to do that to fix some problems. 
	//but beware that it i figure it might could CAUSE some problems

	//this alpha compositing blending logic isnt thought through very much
	//someone needs to think about what bitdepth it should take place at and how to do it efficiently

	for(int i = start, j=0; i <= end_inclusive; ++i, ++j)
	{
	//	u32 stencil = screenStencil[i];

		//you would use this if you wanted to use the stencil buffer to make decisions here
	//	if(!stencil) continue;

	//	u16 oldcolor = dst[j];
		
		int t=i<<2;
	//	u32 dstpixel;

		dst[j] = (screen3D[t+2] | (screen3D[t+1] << 5) | (screen3D[t+0] << 10) | ((screen3D[t+3] > 0) ? 0x8000 : 0x0000));
		dstAlpha[j] = (screen3D[t+3] / 2);
		
		//old debug reminder: display alpha channel
		//u32 r = screen3D[t+3];
		//u32 g = screen3D[t+3];
		//u32 b = screen3D[t+3];

		//if this math strikes you as wrong, be sure to look at GL_ReadFramebuffer() where the pixel format in screen3D is changed

	/*	u32 a = screen3D[t+3];
		
		typedef u8 mixtbl[32][32];
		mixtbl & mix = mixTable555[a];
		
		//r
		u32 newpix = screen3D[t+2];
		u32 oldpix = oldcolor&0x1F;
		newpix = mix[newpix][oldpix];
		dstpixel = newpix;
		
		//g
		newpix = screen3D[t+1];
		oldpix = (oldcolor>>5)&0x1F;
		newpix = mix[newpix][oldpix];
		dstpixel |= (newpix<<5);

		//b
		newpix = screen3D[t+0];
		oldpix = (oldcolor>>10)&0x1F;
		newpix = mix[newpix][oldpix];
		dstpixel |= (newpix<<10);

		dst[j] = dstpixel;*/
	}
}





GPU3DInterface gpu3Dgl = {
	"OpenGL",
	OGLInit,
	OGLReset,
	OGLClose,
	OGLRender,
	OGLVramReconfigureSignal,
	OGLGetLine,
	OGLGetLineCaptured
};



