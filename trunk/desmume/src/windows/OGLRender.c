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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef DESMUME_COCOA
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <gl/gl.h>
	#include <gl/glext.h>
#else
	#define __forceinline
	#include <OpenGL/gl.h>
	#include <OpenGL/glext.h>
#endif

#include "../debug.h"
#include "../MMU.h"
#include "../bits.h"
#include "../matrix.h"
#include "OGLRender.h"


#define fix2float(v)    (((float)((s32)(v))) / (float)(1<<12))
#define fix10_2float(v) (((float)((s32)(v))) / (float)(1<<9))

static unsigned char  GPU_screen3D		[256*256*3]={0};
static float		  GPU_screen3Ddepth	[256*256]={0};

// Acceleration tables
static float*		float16table = NULL;
static float*		float10Table = NULL;
static float*		float10RelTable = NULL;
static float*		normalTable = NULL;
static int			numVertex = 0;

// Matrix stack handling
static __declspec(align(16)) MatrixStack	mtxStack[4];
static __declspec(align(16)) float		mtxCurrent [4][16];
static __declspec(align(16)) float		mtxTemporal[16];

// Indexes for matrix loading/multiplication
static char ML4x4ind = 0;
static char ML4x3_c = 0, ML4x3_l = 0;
static char MM4x4ind = 0;
static char MM4x3_c = 0, MM4x3_l = 0;
static char MM3x3_c = 0, MM3x3_l = 0;

// Data for vertex submission
static __declspec(align(16)) float	coord[4] = {0.0, 0.0, 0.0, 0.0};
static char		coordind = 0;

// Data for basic transforms
static __declspec(align(16)) float	trans[4] = {0.0, 0.0, 0.0, 0.0};
static char		transind = 0;

static __declspec(align(16)) float	scale[4] = {0.0, 0.0, 0.0, 0.0};
static char		scaleind = 0;

static const unsigned short polyType[4] = {GL_TRIANGLES, GL_QUADS, GL_TRIANGLE_STRIP, GL_QUAD_STRIP};
static const unsigned short map3d_cull[4] = {GL_FRONT_AND_BACK, GL_FRONT, GL_BACK, 0};
static const int texEnv[4] = { GL_MODULATE, GL_DECAL, GL_MODULATE, GL_MODULATE };
static const int depthFunc[2] = { GL_LESS, GL_EQUAL };

static unsigned short matrixMode[2] = {GL_PROJECTION, GL_MODELVIEW};
static short mode = 0;

static int colorAlpha=0;
static unsigned int depthFuncMode=0;
static unsigned int lightMask=0;
static unsigned int envMode=0;
static unsigned int cullingMask=0;
static unsigned char texMAP[1024*2048*4]; 
static float fogColor[4] = {0.f};
static float fogOffset = 0.f;
static float alphaTestRef = 0.01f;

static float	alphaTestBase = 0.1f;
static unsigned long clCmd = 0;
static unsigned long clInd = 0;
static unsigned long clInd2 = 0;
static int alphaDepthWrite = 0;
static int colorRGB[4] = { 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff};
static int texCoordinateTransform = 0;
static int _t=0, _s=0;
static char beginCalled = 0;
static unsigned int vtxFormat;
static unsigned int old_vtxFormat;
static unsigned int textureFormat=0, texturePalette=0;
static unsigned int textureMode=0;

static int			diffuse[4] = {0}, 
					ambient[4] = {0}, 
					specular[4] = {0}, 
					emission[4] = {0};

//================================================= Textures
#define MAX_TEXTURE 500
typedef struct
{
	GLenum				id;
	unsigned int		frm;
	unsigned int		mode;
	unsigned int		numcolors;
	unsigned int		pal;
	unsigned int		sizeX;
	unsigned int		sizeY;
	int					coord;
	unsigned int		texenv;
	float				invSizeX;
	float				invSizeY;
	unsigned char		texture[128*1024]; // 128Kb texture slot

} TextureCache;

TextureCache	texcache[MAX_TEXTURE+1];
u32				texcache_count;

u32				texcache_start;
u32				texcache_stop;
//u32				texcache_last;

GLenum			oglTempTextureID[MAX_TEXTURE];
//=================================================

typedef struct
{
	unsigned int color;		// Color in hardware format
	unsigned int direction;	// Direction in hardware format
} LightInformation;

LightInformation g_lightInfo[4] = { 0 };

#ifndef DESMUME_COCOA
extern HWND		hwnd;

int CheckHardwareSupport(HDC hdc)
{
   int PixelFormat = GetPixelFormat(hdc);
   PIXELFORMATDESCRIPTOR pfd;

   DescribePixelFormat(hdc,PixelFormat,sizeof(PIXELFORMATDESCRIPTOR),&pfd);
   if ((pfd.dwFlags & PFD_GENERIC_FORMAT) && !(pfd.dwFlags & PFD_GENERIC_ACCELERATED))
      return 0; // Software acceleration OpenGL

   else if ((pfd.dwFlags & PFD_GENERIC_FORMAT) && (pfd.dwFlags & PFD_GENERIC_ACCELERATED))
      return 1; // Half hardware acceleration OpenGL (MCD driver)

   else if ( !(pfd.dwFlags & PFD_GENERIC_FORMAT) && !(pfd.dwFlags & PFD_GENERIC_ACCELERATED))
      return 2; // Full hardware acceleration OpenGL
   return -1; // check error
}
#endif

__forceinline void NDS_3D_Reset()
{
	int i;

	memset(&texcache,0,sizeof(texcache));
	texcache_count=0;
	for (i = 0; i < MAX_TEXTURE; i++)
		texcache[i].id=oglTempTextureID[i];
	texcache_start=0;
	texcache_stop=MAX_TEXTURE<<1;
}

char NDS_glInit(void)
{
	int i;

#ifndef DESMUME_COCOA
	HDC						oglDC = NULL;
	HGLRC					hRC = NULL;
	int						pixelFormat;
	PIXELFORMATDESCRIPTOR	pfd;
	int						res;
	char					*opengl_modes[3]={"software","half hardware (MCD driver)","hardware"};

	oglDC = GetDC (hwnd);

	memset(&pfd,0, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags =  PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE ;

	pixelFormat = ChoosePixelFormat(oglDC, &pfd);
	if (pixelFormat == 0)
		return 0;

	if(!SetPixelFormat(oglDC, pixelFormat, &pfd))
		return 0;

	hRC = wglCreateContext(oglDC);
	if (!hRC)
		return 0;

	if(!wglMakeCurrent(oglDC, hRC))
		return 0;
	res=CheckHardwareSupport(oglDC);
	if (res>=0&&res<=2) 
			printlog("OpenGL mode: %s\n",opengl_modes[res]); 
		else 
			printlog("OpenGL mode: uknown\n");
#endif
	glClearColor	(0.f, 0.f, 0.f, 1.f);
	glColor3f		(1.f, 1.f, 1.f);

	glEnable		(GL_NORMALIZE);
	glEnable		(GL_DEPTH_TEST);
	glEnable		(GL_TEXTURE_2D);

	glAlphaFunc		(GL_GREATER, 0.1f);
	glEnable		(GL_ALPHA_TEST);

	glGenTextures (MAX_TEXTURE, &oglTempTextureID[0]);

	glViewport(0, 0, 256, 192);
	if (glGetError() != GL_NO_ERROR)
		return 0;

	// Precalculate some tables, to avoid pushing data to the FPU and back for conversion
	float16table = (float*) malloc (sizeof(float)*65536);
	for (i = 0; i < 65536; i++)
	{
		float16table[i] = fix2float((signed short)i);
	}

	float10RelTable = (float*) malloc (sizeof(float)*1024);
	for (i = 0; i < 1024; i++)
	{
		float10RelTable[i] = ((signed short)(i<<6)) / (float)(1<<18);
	}

	float10Table = (float*) malloc (sizeof(float)*1024);
	for (i = 0; i < 1024; i++)
	{
		float10Table[i] = ((signed short)(i<<6)) / (float)(1<<12);
	}

	normalTable = (float*) malloc (sizeof(float)*1024);
	for (i = 0; i < 1024; i++)
	{
		normalTable[i] = ((signed short)(i<<6)) / (float)(1<<16);
	}

	MatrixStackSetMaxSize(&mtxStack[0], 1);		// Projection stack
	MatrixStackSetMaxSize(&mtxStack[1], 31);	// Coordinate stack
	MatrixStackSetMaxSize(&mtxStack[2], 31);	// Directional stack
	MatrixStackSetMaxSize(&mtxStack[3], 1);		// Texture stack

	MatrixInit (mtxCurrent[0]);
	MatrixInit (mtxCurrent[1]);
	MatrixInit (mtxCurrent[2]);
	MatrixInit (mtxCurrent[3]);
	MatrixInit (mtxTemporal);

	NDS_3D_Reset();

	return 1;
}

__forceinline void NDS_3D_Close()
{
}

__forceinline void SetMaterialAlpha (int alphaValue)
{
	diffuse[3]	= alphaValue;
	ambient[3]	= alphaValue;
	specular[3] = alphaValue;
	emission[3] = alphaValue;

	glMaterialiv (GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
	glMaterialiv (GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);

	glMaterialiv (GL_FRONT_AND_BACK, GL_SPECULAR, specular);
	glMaterialiv (GL_FRONT_AND_BACK, GL_EMISSION, emission);
}

__forceinline void NDS_glViewPort(unsigned long v)
{
	if(beginCalled)
		glEnd();

	glViewport( (v&0xFF), ((v>>8)&0xFF), ((v>>16)&0xFF), ((v>>24)&0xFF));

	if(beginCalled)
		glBegin(vtxFormat);
}

__forceinline void NDS_glClearColor(unsigned long v)
{
	if(beginCalled)
		glEnd();

	glClearColor(	((float)(v&0x1F))/31.0f,
					((float)((v>>5)&0x1F))/31.0f,
					((float)((v>>10)&0x1F))/31.0f,
					((float)((v>>16)&0x1F))/31.0f);

	if(beginCalled)
		glBegin(vtxFormat);
}

__forceinline void NDS_glFogColor(unsigned long v)
{
	fogColor[0] = ((float)((v    )&0x1F))/31.0f;
	fogColor[1] = ((float)((v>> 5)&0x1F))/31.0f;
	fogColor[2] = ((float)((v>>10)&0x1F))/31.0f;
	fogColor[3] = ((float)((v>>16)&0x1F))/31.0f;
}

__forceinline void NDS_glFogOffset (unsigned long v)
{
	fogOffset = (float)(v&0xffff);
}

__forceinline void NDS_glClDepth()
{
	if(beginCalled)
		glEnd();

	glClear(GL_DEPTH_BUFFER_BIT);

	if(beginCalled)
		glBegin(vtxFormat);
}

__forceinline void NDS_glClearDepth(unsigned long v)
{
	u32 depth24b;

	if(beginCalled)
		glEnd();

	v		&= 0x7FFFF;
	// Thanks for NHerve
	depth24b = (v*0x200)+((v+1)/0x8000)*0x01FF;
	glClearDepth(depth24b / ((float)(1<<24)));

	if(beginCalled)
		glBegin(vtxFormat);
}

__forceinline void NDS_glMatrixMode(unsigned long v)
{
	if (beginCalled)
	{
		glEnd();
	}

	mode = (short)(v&3);

	if (beginCalled)
	{
		glBegin (vtxFormat);
	}
}

__forceinline void SetMatrix (void)
{
	if (mode < 2)
	{
		if (beginCalled)
			glEnd();
		glMatrixMode (matrixMode[mode]);
		glLoadMatrixf(mtxCurrent[mode]);
		glBegin (vtxFormat);
		beginCalled = 1;
	}
	else if (mode == 2)
	{
		if (beginCalled)
			glEnd();
		glMatrixMode (matrixMode[1]);
		glLoadMatrixf(mtxCurrent[1]);
		glBegin (vtxFormat);
		beginCalled = 1;
	}
}

__forceinline void NDS_glLoadIdentity (void)
{
	MatrixIdentity (mtxCurrent[mode]);

	if (mode == 2)
		MatrixIdentity (mtxCurrent[1]);
}

__forceinline void NDS_glLoadMatrix4x4(signed long v)
{
	mtxCurrent[mode][ML4x4ind] = fix2float(v);

	++ML4x4ind;
	if(ML4x4ind<16) return;

	if (mode == 2)
		MatrixCopy (mtxCurrent[1], mtxCurrent[2]);

	ML4x4ind = 0;
}

__forceinline void NDS_glLoadMatrix4x3(signed long v)
{
	mtxCurrent[mode][(ML4x3_l<<2)+ML4x3_c] = fix2float(v);

	++ML4x3_c;
	if(ML4x3_c<3) return;

	ML4x3_c = 0;
	++ML4x3_l;
	if(ML4x3_l<4) return;
	ML4x3_l = 0;

	if (mode == 2)
		MatrixCopy (mtxCurrent[1], mtxCurrent[2]);
}

__forceinline void NDS_glStoreMatrix(unsigned long v)
{
	//this command always works on both pos and vector when either pos or pos-vector are the current mtx mode
	short mymode = (mode==1?2:mode);

	//for the projection matrix, the provided value is supposed to be reset to zero
	if(mymode==0) 
		v = 0;
	
	MatrixStackLoadMatrix (&mtxStack[mymode], v&31, mtxCurrent[mymode]);
	if(mymode==2)
		MatrixStackLoadMatrix (&mtxStack[1], v&31, mtxCurrent[1]);
}

__forceinline void NDS_glRestoreMatrix(unsigned long v)
{
	//this command always works on both pos and vector when either pos or pos-vector are the current mtx mode
	short mymode = (mode==1?2:mode);

	//for the projection matrix, the provided value is supposed to be reset to zero
	if(mymode==0)
		v = 0;

	MatrixCopy (mtxCurrent[mymode], MatrixStackGetPos(&mtxStack[mymode], v&31));
	if (mymode == 2)
		MatrixCopy (mtxCurrent[1], MatrixStackGetPos(&mtxStack[1], v&31));
}

__forceinline void NDS_glPushMatrix (void)
{
		//this command always works on both pos and vector when either pos or pos-vector are the current mtx mode
	short mymode = (mode==1?2:mode);

	MatrixStackPushMatrix (&mtxStack[mymode], mtxCurrent[mymode]);
	if(mymode==2)
		MatrixStackPushMatrix (&mtxStack[1], mtxCurrent[1]);
}

__forceinline void NDS_glPopMatrix(signed long i)
{
	//this command always works on both pos and vector when either pos or pos-vector are the current mtx mode
	short mymode = (mode==1?2:mode);

	MatrixCopy (mtxCurrent[mode], MatrixStackPopMatrix (&mtxStack[mode], i));

	if (mymode == 2)
		MatrixCopy (mtxCurrent[1], MatrixStackPopMatrix (&mtxStack[1], i));
}

__forceinline void NDS_glTranslate(signed long v)
{
	trans[transind] = fix2float(v);

	++transind;

	if(transind<3)
		return;

	transind = 0;

	MatrixTranslate (mtxCurrent[mode], trans);

	if (mode == 2)
		MatrixTranslate (mtxCurrent[1], trans);
}

__forceinline void NDS_glScale(signed long v)
{
	short mymode = (mode==2?1:mode);
	
	scale[scaleind] = fix2float(v);

	++scaleind;

	if(scaleind<3)
		return;

	scaleind = 0;

	MatrixScale (mtxCurrent[mymode], scale);

	//note: pos-vector mode should not cause both matrices to scale.
	//the whole purpose is to keep the vector matrix orthogonal
	//so, I am leaving this commented out as an example of what not to do.
	//if (mode == 2)
	//	MatrixScale (mtxCurrent[1], scale);
}

__forceinline void NDS_glMultMatrix3x3(signed long v)
{
	mtxTemporal[(MM3x3_l<<2)+MM3x3_c] = fix2float(v);

	++MM3x3_c;
	if(MM3x3_c<3) return;

	MM3x3_c = 0;
	++MM3x3_l;
	if(MM3x3_l<3) return;
	MM3x3_l = 0;

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);

	if (mode == 2)
		MatrixMultiply (mtxCurrent[1], mtxTemporal);

	MatrixIdentity (mtxTemporal);
}

__forceinline void NDS_glMultMatrix4x3(signed long v)
{
	mtxTemporal[(MM4x3_l<<2)+MM4x3_c] = fix2float(v);

	++MM4x3_c;
	if(MM4x3_c<3) return;

	MM4x3_c = 0;
	++MM4x3_l;
	if(MM4x3_l<4) return;
	MM4x3_l = 0;

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);
	if (mode == 2)
		MatrixMultiply (mtxCurrent[1], mtxTemporal);

	MatrixIdentity (mtxTemporal);
}

__forceinline void NDS_glMultMatrix4x4(signed long v)
{
	mtxTemporal[MM4x4ind] = fix2float(v);

	++MM4x4ind;
	if(MM4x4ind<16) return;

	MM4x4ind = 0;

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);
	if (mode == 2)
		MatrixMultiply (mtxCurrent[1], mtxTemporal);

	MatrixIdentity (mtxTemporal);
}

//zero 8/25/08 - i dont like this
//#define CHECKSLOT txt_slot_current_size--;\
//					if (txt_slot_current_size<=0)\
//					{\
//						txt_slot_current++;\
//						*adr=(unsigned char *)ARM9Mem.textureSlotAddr[txt_slot_current];\
//						adr-=txt_slot_size;\
//						txt_slot_size=txt_slot_current_size=0x020000;\
//					}

#define CHECKSLOT txt_slot_current_size--;\
					if (txt_slot_current_size<=0)\
					{\
						txt_slot_current++;\
						*adr=(unsigned char *)ARM9Mem.textureSlotAddr[txt_slot_current];\
						txt_slot_size=txt_slot_current_size=0x020000;\
					}


#define RGB16TO32(col,alpha) (((alpha)<<24) | ((((col) & 0x7C00)>>7)<<16) | ((((col) & 0x3E0)>>2)<<8) | (((col) & 0x1F)<<3))

__forceinline void* memcpy_fast(void* dest, const void* src, size_t count)
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
//================================================================================
__forceinline void setTexture(unsigned int format, unsigned int texpal)
{
	int palSize[7]={32,4,16,256,0,8,32768};
	int i=0;
	unsigned int x=0, y=0;
	unsigned int palZeroTransparent;
	
	unsigned short *pal = NULL;
	unsigned char *dst = texMAP;
	unsigned int sizeX=(8 << ((format>>20)&0x07));
	unsigned int sizeY=(8 << ((format>>23)&0x07));
	unsigned int imageSize = sizeX*sizeY;

	u64 txt_slot_current_size;
	u64 txt_slot_size;
	u64 txt_slot_current;
	unsigned char * adr;

	textureMode = (unsigned short)((format>>26)&0x07);

	if (format==0)
	{
		texcache_count=-1;
		return;
	}
	if (textureMode==0)
	{
		texcache_count=-1;
		return;
	}

	txt_slot_current=(format>>14)&0x03;
	adr=(unsigned char *)(ARM9Mem.textureSlotAddr[txt_slot_current]+((format&0x3FFF)<<3));
	
	i=texcache_start;
	while (TRUE)
	{
		if (texcache_stop==i) break;
		if (texcache[i].frm==0) break;
		if ((texcache[i].frm==format)&&(texcache[i].pal==texpal))
		{
			if (!memcmp(adr,texcache[i].texture,imageSize))
			{
				texcache_count=i;
				glBindTexture(GL_TEXTURE_2D,texcache[i].id);
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

	glBindTexture(GL_TEXTURE_2D, texcache[i].id);
	texcache[i].mode=textureMode;
	texcache[i].pal=texpal;
	texcache[i].sizeX=sizeX;
	texcache[i].sizeY=sizeY;
	texcache[i].coord=(format>>30);
	texcache[i].invSizeX=1.0f/((float)sizeX*(1<<4));
	texcache[i].invSizeY=1.0f/((float)sizeY*(1<<4));
	texcache[i].texenv=envMode;
	//memcpy(texcache[i].texture,adr,imageSize);			//======================= copy
	memcpy_fast(texcache[i].texture,adr,imageSize);			//======================= copy
	texcache[i].numcolors=palSize[texcache[i].mode];

	texcache[i].frm=format;

	//printlog("Texture %03i - format=%08X; pal=%04X (mode %X, width %04i, height %04i)\n",i, texcache[i].frm, texcache[i].pal, texcache[i].mode, sizeX, sizeY);
	
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
					unsigned short c = pal[adr[x]&31], alpha = (adr[x]>>5);
					dst[0] = (unsigned char)((c & 0x1F)<<3);
					dst[1] = (unsigned char)((c & 0x3E0)>>2);
					dst[2] = (unsigned char)((c & 0x7C00)>>7);
					dst[3] = ((alpha<<2)+(alpha>>1))<<3;
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
						unsigned short c = pal[adr[x]];
						dst[0] = (unsigned char)((c & 0x1F)<<3);
						dst[1] = (unsigned char)((c & 0x3E0)>>2);
						dst[2] = (unsigned char)((c & 0x7C00)>>7);
						dst[3] = (adr[x] == 0) ? palZeroTransparent : 255;
						dst += 4;
						CHECKSLOT;
					}
				}
				break;
			case 5: //4x4
			{
				unsigned short * pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
				unsigned short * slot1;
				unsigned int * map = (unsigned int *)adr;
				unsigned d = 0;
				unsigned int * dst = (unsigned int *)texMAP;
				if ( (texcache[i].frm & 0xc000) == 0x8000)
					// texel are in slot 2
					slot1=(const unsigned short*)&ARM9Mem.textureSlotAddr[1][((texcache[i].frm&0x3FFF)<<2)+0x010000];
				else 
					slot1=(const unsigned short*)&ARM9Mem.textureSlotAddr[1][(texcache[i].frm&0x3FFF)<<2];

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

								dst[currentPos] = tmp_col[currRow&3];
								dst[currentPos+1] = tmp_col[(currRow>>2)&3];
								dst[currentPos+2] = tmp_col[(currRow>>4)&3];
								dst[currentPos+3] = tmp_col[(currRow>>6)&3];
								
								txt_slot_current_size-=4;;
								if (txt_slot_current_size<=0)
								{
									txt_slot_current++;
									*map=(unsigned char *)ARM9Mem.textureSlotAddr[txt_slot_current];
									//map-=txt_slot_size>>2; //zero 8/25/08 - I dont understand this. it broke my game.
									txt_slot_size=txt_slot_current_size=0x020000;
								}
							}
						}
					}

			//zero debug - dump tex4x4 to verify contents
		/*	{
				int NDS_WriteBMP_32bppBuffer(int width, int height, const void* buf, const char *filename);
				static int ctr = 0;
				char fname[100];
				FILE* outf;
				sprintf(fname,"c:\\dump\\%d.bmp", ctr);
				ctr++;
				NDS_WriteBMP_32bppBuffer(sizeX,sizeY,texMAP,fname);
			}*/

				break;
			}
			case 6: //a5i3
			{
				pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
				for(x = 0; x < imageSize; x++)
				{
					unsigned short c = pal[adr[x]&0x07];
					dst[0] = (unsigned char)((c & 0x1F)<<3);
					dst[1] = (unsigned char)((c & 0x3E0)>>2);
					dst[2] = (unsigned char)((c & 0x7C00)>>7);
					dst[3] = (adr[x]&0xF8);
					dst += 4;
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
					unsigned short c = map[x];
					dst[0] = ((c & 0x1F)<<3);
					dst[1] = ((c & 0x3E0)>>2);
					dst[2] = ((c & 0x7C00)>>7);
					dst[3] = (c>>15)*255;
					
					dst += 4;
					txt_slot_current_size-=2;;
					if (txt_slot_current_size<=0)
					{
						txt_slot_current++;
						*map=(unsigned char *)ARM9Mem.textureSlotAddr[txt_slot_current];
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
	//============================================================================================
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, texEnv[texcache[i].texenv]);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (BIT16(texcache[i].frm) ? (BIT18(texcache[i].frm)?GL_MIRRORED_REPEAT:GL_REPEAT) : GL_CLAMP));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (BIT17(texcache[i].frm) ? (BIT18(texcache[i].frm)?GL_MIRRORED_REPEAT:GL_REPEAT) : GL_CLAMP));
	texcache_count=i;
}

__forceinline void NDS_glBegin(unsigned long v)
{
	int enableDepthWrite = 1;

	u32 tmp=0;

	if (beginCalled)
	{
		glEnd();
	}

	// Light enable/disable
	if (lightMask)
	{
		glEnable (GL_LIGHTING);
		(lightMask&0x01)?glEnable (GL_LIGHT0):glDisable(GL_LIGHT0);
		(lightMask&0x02)?glEnable (GL_LIGHT1):glDisable(GL_LIGHT1);
		(lightMask&0x04)?glEnable (GL_LIGHT2):glDisable(GL_LIGHT2);
		(lightMask&0x08)?glEnable (GL_LIGHT3):glDisable(GL_LIGHT3);
	}
	else
		glDisable (GL_LIGHTING);

	glDepthFunc (depthFuncMode);

	// Cull face
	if (cullingMask != 0xC0)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(map3d_cull[cullingMask>>6]);
	}
	else
		glDisable(GL_CULL_FACE);

	// Alpha value, actually not well handled, 0 should be wireframe
	if (colorAlpha > 0.0f)
	{
		glPolygonMode (GL_FRONT, GL_FILL);
		glPolygonMode (GL_BACK, GL_FILL);

		if (lightMask)
		{
			SetMaterialAlpha (colorAlpha);
		}
		else
		{
			colorRGB[3] = colorAlpha;
			glColor4iv ((GLint*)colorRGB);
		}

		//non-31 alpha polys are translucent
		if(colorAlpha != 0x7C000000)
			enableDepthWrite = alphaDepthWrite;
	}
	else
	{
		glPolygonMode (GL_FRONT, GL_LINE);
		glPolygonMode (GL_BACK, GL_LINE);
	}

	// texture environment
	setTexture(textureFormat, texturePalette);
	//=================
	if (texcache_count!=-1)
	{
		texCoordinateTransform = texcache[texcache_count].coord;

		glMatrixMode (GL_TEXTURE);
		glLoadIdentity ();
		glScaled (texcache[texcache_count].invSizeX, texcache[texcache_count].invSizeY, 1.0f);
	}

	//a5i3 or a3i5 textures are translucent
	alphaDepthWrite = 0; //zero - as a hack, we are never going to write depth buffer for alpha values
	//this is the best we can do right now until we can sort and display translucent polys last like we're supposed to
	//(here is a sample case which illustrates the problem)
	//1. draw something opaque in the distance
	//2. draw something translucent in the foreground
	//3. draw something opaque in the middle ground
	if(textureMode ==1 || textureMode == 6)
		enableDepthWrite = alphaDepthWrite;

	glDepthMask(enableDepthWrite?GL_TRUE:GL_FALSE);

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(mtxCurrent[0]);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();


	beginCalled = 1;
	vtxFormat = polyType[v&0x03];
	glBegin(vtxFormat);
}

__forceinline void NDS_glEnd (void)
{
	if (beginCalled)
	{
		glEnd();
		beginCalled = 0;
	}
}

__forceinline void NDS_glColor3b(unsigned long v)
{
	colorRGB[0] =  (v&0x1F) << 26;
	colorRGB[1] = ((v>>5)&0x1F) << 26;
	colorRGB[2] = ((v>>10)&0x1F) << 26;
	glColor4iv ((GLint*)colorRGB);
}

static __forceinline void  SetVertex()
{
	__declspec(align(16)) float coordTransformed[3] = { coord[0], coord[1], coord[2] };

	if (texCoordinateTransform == 3)
	{
		float s2 =((coord[0]*mtxCurrent[3][0] +
					coord[1]*mtxCurrent[3][4] +
					coord[2]*mtxCurrent[3][8]) + _s);
		float t2 =((coord[0]*mtxCurrent[3][1] +
					coord[1]*mtxCurrent[3][5] +
					coord[2]*mtxCurrent[3][9]) + _t);

		glTexCoord2f (s2, t2);
		//return;
	}

	MatrixMultVec4x4 (mtxCurrent[1], coordTransformed);

	glVertex3fv (coordTransformed);

	//zero - helpful in making sure vertex colors or lighting arent broken
	//glColor3ub(rand()&255,rand()&255,rand()&255);

	numVertex++;
}

__forceinline void NDS_glVertex16b(unsigned int v)
{
	if(coordind==0)
	{
		coord[0]		= float16table[v&0xFFFF];
		coord[1]		= float16table[v>>16];

		++coordind;
		return;
	}

	coord[2]	  = float16table[v&0xFFFF];

	coordind = 0;
	SetVertex ();
}

__forceinline void NDS_glVertex10b(unsigned long v)
{
	coord[0]		= float10Table[v&1023];
	coord[1]		= float10Table[(v>>10)&1023];
	coord[2]		= float10Table[(v>>20)&1023];

	SetVertex ();
}

__forceinline void NDS_glVertex3_cord(unsigned int one, unsigned int two, unsigned int v)
{
	coord[one]		= float16table[v&0xffff];
	coord[two]		= float16table[v>>16];

	SetVertex ();
}

__forceinline void NDS_glVertex_rel(unsigned long v)
{
	coord[0]		+= float10RelTable[v&1023];
	coord[1]		+= float10RelTable[(v>>10)&1023];
	coord[2]		+= float10RelTable[(v>>20)&1023];

	SetVertex ();
}

// Ignored for now
__forceinline void NDS_glSwapScreen(unsigned int screen)
{
}


// THIS IS A HACK :D
__forceinline int NDS_glGetNumPolys (void)
{
	return numVertex/3;
}

__forceinline int NDS_glGetNumVertex (void)
{
	return numVertex;
}

__forceinline void NDS_glGetLine (int line, unsigned short * dst)
{
	int		i, t;
	u8		*screen3D		= (u8 *)&GPU_screen3D	[(192-(line%192))*768];
	float	*screen3Ddepth	= &GPU_screen3Ddepth	[(192-(line%192))*256];
	u32		r,g,b;

	for(i = 0, t=0; i < 256; i++)
	{
		if (screen3Ddepth[i] < 1.f)
		{
			t=i*3;
			r = screen3D[t];
			g = screen3D[t+1];
			b = screen3D[t+2];

			dst[i] = ((r>>3)<<10) | ((g>>3)<<5) | (b>>3);
		}
	}
}

__forceinline void NDS_glFlush(unsigned long v)
{
	if (beginCalled)
	{
		glEnd();
		beginCalled = 0;
	}

	clCmd = 0;
	clInd = 0;

	glFlush();
	glReadPixels(0,0,256,192,GL_DEPTH_COMPONENT,	GL_FLOAT,			GPU_screen3Ddepth);
	glReadPixels(0,0,256,192,GL_BGR_EXT,			GL_UNSIGNED_BYTE,	GPU_screen3D);	

	numVertex = 0;

	// Set back some secure render states
	glPolygonMode	(GL_BACK,  GL_FILL);
	glPolygonMode	(GL_FRONT, GL_FILL);

	glDepthMask		(GL_TRUE);
	glClear			(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

__forceinline void NDS_glPolygonAttrib (unsigned long val)
{

	// Light enable/disable
	lightMask = (val&0xF);

	// texture environment
    //envMode = texEnv[(val&0x30)>>4];
	envMode = (val&0x30)>>4;

	// overwrite depth on alpha pass
	alphaDepthWrite = BIT11(val);

	// depth test function
	depthFuncMode = depthFunc[BIT14(val)];

	// back face culling
	cullingMask = (val&0xC0);

	// Alpha value, actually not well handled, 0 should be wireframe
	colorAlpha = ((val>>16)&0x1F)<<26;
	//printlog("PolygonID=%i;\n",val>>24);
}

/*
	0-4   Diffuse Reflection Red
	5-9   Diffuse Reflection Green
	10-14 Diffuse Reflection Blue
	15    Set Vertex Color (0=No, 1=Set Diffuse Reflection Color as Vertex Color)
	16-20 Ambient Reflection Red
	21-25 Ambient Reflection Green
	26-30 Ambient Reflection Blue
*/
__forceinline void NDS_glMaterial0 (unsigned long val)
{
	diffuse[0] = ((val&0x1F) << 26) | 0x03FFFFFF;
	diffuse[1] = (((val>>5)&0x1F) << 26) | 0x03FFFFFF;
	diffuse[2] = (((val>>10)&0x1F) << 26) | 0x03FFFFFF;
	diffuse[3] = 0x7fffffff;

	ambient[0] = (((val>>16)&0x1F) << 26) | 0x03FFFFFF;
	ambient[1] = (((val>>21)&0x1F) << 26) | 0x03FFFFFF;
	ambient[2] = (((val>>26)&0x1F) << 26) | 0x03FFFFFF;
	ambient[3] = 0x7fffffff;

	if (BIT15(val))
	{
		colorRGB[0] = diffuse[0];
		colorRGB[1] = diffuse[1];
		colorRGB[2] = diffuse[2];
	}

	if (beginCalled)
	{
		glEnd();
	}

	glMaterialiv (GL_FRONT_AND_BACK, GL_AMBIENT, (GLint*)ambient);
	glMaterialiv (GL_FRONT_AND_BACK, GL_DIFFUSE, (GLint*)diffuse);

	if (beginCalled)
	{
		glBegin (vtxFormat);
	}
}

__forceinline void NDS_glMaterial1 (unsigned long val)
{
	specular[0] = 	((val&0x1F) << 26) | 0x03FFFFFF;
	specular[1] = 	(((val>>5)&0x1F) << 26) | 0x03FFFFFF;
	specular[2] = 	(((val>>10)&0x1F) << 26) | 0x03FFFFFF;
	specular[3] = 	0x7fffffff;

	emission[0] = 	(((val>>16)&0x1F) << 26) | 0x03FFFFFF;
	emission[1] = 	(((val>>21)&0x1F) << 26) | 0x03FFFFFF;
	emission[2] = 	(((val>>26)&0x1F) << 26) | 0x03FFFFFF;
	emission[3] = 	0x7fffffff;

	if (beginCalled)
	{
		glEnd();
	}

	glMaterialiv (GL_FRONT_AND_BACK, GL_SPECULAR, (GLint*)specular);
	glMaterialiv (GL_FRONT_AND_BACK, GL_EMISSION, (GLint*)emission);

	if (beginCalled)
	{
		glBegin (vtxFormat);
	}
}

__forceinline void NDS_glShininess (unsigned long val)
{
	//printlog("Shininess=%i\n",val);
}

__forceinline void NDS_glTexImage(unsigned long val)
{
	textureFormat = val;
}

__forceinline void NDS_glTexPalette(unsigned long val)
{
	texturePalette = val;
}

__forceinline void NDS_glTexCoord(unsigned long val)
{
	_t = (s16)(val>>16);
	_s = (s16)(val&0xFFFF);

	if (texCoordinateTransform == 1)
	{
		float s2, t2;
		s2 =_s*mtxCurrent[3][0] + _t*mtxCurrent[3][4] +
				0.0625f*mtxCurrent[3][8] + 0.0625f*mtxCurrent[3][12];
		t2 =_s*mtxCurrent[3][1] + _t*mtxCurrent[3][5] +
				0.0625f*mtxCurrent[3][9] + 0.0625f*mtxCurrent[3][13];

		glTexCoord2f (s2, t2);
		return;
	}
	glTexCoord2f (_s, _t);
}

__forceinline signed long NDS_glGetClipMatrix (unsigned int index)
{
	float val = MatrixGetMultipliedIndex (index, mtxCurrent[0], mtxCurrent[1]);

	val *= (1<<12);

	return (signed long)val;
}

__forceinline signed long NDS_glGetDirectionalMatrix (unsigned int index)
{
	index += (index/3);

	return (signed long)(mtxCurrent[2][(index)*(1<<12)]);
}


/*
	0-9   Directional Vector's X component (1bit sign + 9bit fractional part)
	10-19 Directional Vector's Y component (1bit sign + 9bit fractional part)
	20-29 Directional Vector's Z component (1bit sign + 9bit fractional part)
	30-31 Light Number                     (0..3)
*/

__forceinline void NDS_glLightDirection (unsigned long v)
{
	int		index = v>>30;
	float	direction[4];

	if (beginCalled)
		glEnd();

	// Convert format into floating point value
	// and pass it to OpenGL
	direction[0] = -normalTable[v&1023];
	direction[1] = -normalTable[(v>>10)&1023];
	direction[2] = -normalTable[(v>>20)&1023];
	direction[3] = 0;

//	glLightf (GL_LIGHT0 + index, GL_CONSTANT_ATTENUATION, 0);

	glLightfv(GL_LIGHT0 + index, GL_POSITION, direction);

	if (beginCalled)
		glBegin (vtxFormat);

	// Keep information for GetLightDirection function
	g_lightInfo[index].direction = v;
}

__forceinline void NDS_glLightColor (unsigned long v)
{
	int lightColor[4] = {	((v)    &0x1F)<<26,
							((v>> 5)&0x1F)<<26,
							((v>>10)&0x1F)<<26,
							0x7fffffff};
	int index = v>>30;

	if (beginCalled)
		glEnd();

	glLightiv(GL_LIGHT0 + index, GL_AMBIENT, (GLint*)lightColor);
	glLightiv(GL_LIGHT0 + index, GL_DIFFUSE, (GLint*)lightColor);
	glLightiv(GL_LIGHT0 + index, GL_SPECULAR, (GLint*)lightColor);

	if (beginCalled)
		glBegin (vtxFormat);

	// Keep color information for NDS_glGetColor
	g_lightInfo[index].color = v;
}

__forceinline void NDS_glAlphaFunc(unsigned long v)
{
	alphaTestRef = (v&31)/31.f;
}

__forceinline void NDS_glControl(unsigned long v)
{
	if(beginCalled)
		glEnd();

	if(v&1)
	{
		glEnable (GL_TEXTURE_2D);
	}
	else
	{
		glDisable (GL_TEXTURE_2D);
	}

	if(v&(1<<2))
	{
		glAlphaFunc	(GL_GREATER, alphaTestBase);
	}
	else
	{
		glAlphaFunc	(GL_GREATER, 0.1f);
	}

	if(v&(1<<3))
	{
		glBlendFunc		(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable		(GL_BLEND);
	}
	else
	{
		glDisable		(GL_BLEND);
	}

	if (v&(1<<14))
	{
		printlog("Enabled BITMAP background mode\n");
	}

	if(beginCalled)
		glBegin(vtxFormat);
}

__forceinline void NDS_glNormal(unsigned long v)
{

	__declspec(align(16)) float normal[3] = { normalTable[v&1023],
						normalTable[(v>>10)&1023],
						normalTable[(v>>20)&1023]};

	if (texCoordinateTransform == 2)
	{
		float s2 =(	(normal[0] *mtxCurrent[3][0] + normal[1] *mtxCurrent[3][4] +
					 normal[2] *mtxCurrent[3][8]) + _s);
		float t2 =(	(normal[0] *mtxCurrent[3][1] + normal[1] *mtxCurrent[3][5] +
					 normal[2] *mtxCurrent[3][9]) + _t);

		glTexCoord2f (s2, t2);
	}

	MatrixMultVec3x3 (mtxCurrent[2], normal);

	glNormal3fv(normal);
}

__forceinline void NDS_glBoxTest(unsigned long v)
{
}

__forceinline void NDS_glPosTest(unsigned long v)
{
}

__forceinline void NDS_glVecTest(unsigned long v)
{
	//printlog("NDS_glVecTest\n");
}

__forceinline long NDS_glGetPosRes(unsigned int index)
{
	//printlog("NDS_glGetPosRes\n");
	return 0;
}

__forceinline long NDS_glGetVecRes(unsigned int index)
{
	//printlog("NDS_glGetVecRes\n");
	return 0;
}

__forceinline void NDS_glCallList(unsigned long v)
{
	//static unsigned long oldval = 0, shit = 0;

	if(!clInd)
	{
		clInd = 4;
		clCmd = v;
		return;
	}

	for (;;)
	{
		switch ((clCmd&0xFF))
		{
			case 0x0:
			{
				if (clInd > 0)
				{
					--clInd;
					clCmd >>= 8;
					continue;
				}
				break;
			}

			case 0x11 :
			{
				((unsigned long *)ARM9Mem.ARM9_REG)[0x444>>2] = v;
				NDS_glPushMatrix();
				--clInd;
				clCmd>>=8;
				continue;
			}

			case 0x15:
			{
				((unsigned long *)ARM9Mem.ARM9_REG)[0x454>>2] = v;
				NDS_glLoadIdentity();
				--clInd;
				clCmd>>=8;
				continue;
			}

			case 0x41:
			{
				NDS_glEnd();
				--clInd;
				clCmd>>=8;
				continue;
			}
		}

		break;
	}


	if(!clInd)
	{
		clInd = 4;
		clCmd = v;
		return;
	}

	switch(clCmd&0xFF)
	{
		case 0x10:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x440>>2] = v;
			NDS_glMatrixMode (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x12:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x448>>2] = v;
			NDS_glPopMatrix(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x13:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x44C>>2] = v;
			NDS_glStoreMatrix(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x14:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x450>>2] = v;
			NDS_glRestoreMatrix (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x16:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x458>>2] = v;
			NDS_glLoadMatrix4x4(v);
			clInd2++;
			if(clInd2==16)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x17:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x45C>>2] = v;
			NDS_glLoadMatrix4x3(v);
			clInd2++;
			if(clInd2==12)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x18:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x460>>2] = v;
			NDS_glMultMatrix4x4(v);
			clInd2++;
			if(clInd2==16)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x19:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x464>>2] = v;
			NDS_glMultMatrix4x3(v);
			clInd2++;
			if(clInd2==12)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x1A:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x468>>2] = v;
			NDS_glMultMatrix3x3(v);
			clInd2++;
			if(clInd2==9)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x1B:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x46C>>2] = v;
			NDS_glScale (v);
			clInd2++;
			if(clInd2==3)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x1C:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x470>>2] = v;
			NDS_glTranslate (v);
			clInd2++;
			if(clInd2==3)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x20 :
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x480>>2] = v;
			NDS_glColor3b(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x21 :
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x484>>2] = v;
			NDS_glNormal(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x22 :
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x488>>2] = v;
			NDS_glTexCoord(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x23 :
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x48C>>2] = v;
			NDS_glVertex16b(v);
			clInd2++;
			if(clInd2==2)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x24:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x490>>2] = v;
			NDS_glVertex10b(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x25:// GFX_VERTEX_XY
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x494>>2] = v;
			NDS_glVertex3_cord(0,1,v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x26:// GFX_VERTEX_XZ
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x498>>2] = v;
			NDS_glVertex3_cord(0,2,v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x27:// GFX_VERTEX_YZ
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x49C>>2] = v;
			NDS_glVertex3_cord(1,2,v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x28: // GFX_VERTEX_DIFF
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4A0>>2] = v;
			NDS_glVertex_rel (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x29:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4A4>>2] = v;
			NDS_glPolygonAttrib (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x2A:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4A8>>2] = v;
			NDS_glTexImage (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x2B:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4AC>>2] = v;
			NDS_glTexPalette (v&0x1FFF);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x30: // GFX_DIFFUSE_AMBIENT
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4C0>>2] = v;
			NDS_glMaterial0(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x31: // GFX_SPECULAR_EMISSION
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4C4>>2] = v;
			NDS_glMaterial1(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x32:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4C8>>2] = v;
			NDS_glLightDirection(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x33:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4CC>>2] = v;
			NDS_glLightColor(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x34:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4D0>>2] = v;
			NDS_glShininess (v);
			clInd2++;
			if(clInd2==32)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x40 :
		{
			//old_vtxFormat=((unsigned long *)ARM9Mem.ARM9_REG)[0x500>>2];
			((unsigned long *)ARM9Mem.ARM9_REG)[0x500>>2] = v;
			NDS_glBegin(v);
			--clInd;
			clCmd>>=8;
			break;
		}
/*
		case 0x50:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x540>>2] = v;
			NDS_glFlush(v);
			--clInd;
			clCmd>>=8;
			break;
		}
*/
		case 0x60:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x580>>2] = v;
			NDS_glViewPort(v);
			--clInd;
			clCmd>>=8;
			break;
		}
/*
		case 0x80:
		{
			clInd2++;
			if(clInd2==7)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}

			break;
		}
*/
		default:
		{
			LOG ("Unknown 3D command %02X\n", clCmd&0xFF);
			--clInd;
			clCmd>>=8;
			break;
		}
	}
	if((clCmd&0xFF)==0x41)
	{
		if(beginCalled)
		{
			glEnd();
			beginCalled = 0;
		}
		--clInd;
		clCmd>>=8;
	}
}

__forceinline void NDS_glGetMatrix(unsigned int mode, unsigned int index, float* dest)
{
	//int n;

	if(index == -1)
	{
		MatrixCopy(dest, mtxCurrent[mode]);
		return;
	}

	MatrixCopy(dest, MatrixStackGetPos(&mtxStack[mode], index));
}

__forceinline void NDS_glGetLightDirection(unsigned int index, unsigned int* dest)
{
	*dest = g_lightInfo[index].direction;
}

__forceinline void NDS_glGetLightColor(unsigned int index, unsigned int* dest)
{
	*dest = g_lightInfo[index].color;
}

GPU3DInterface gpu3Dgl = {	NDS_glInit,
							NDS_3D_Reset,
							NDS_3D_Close,

							NDS_glViewPort,
							NDS_glClearColor,
							NDS_glFogColor,
							NDS_glFogOffset,
							NDS_glClearDepth,
							NDS_glMatrixMode,
							NDS_glLoadIdentity,
							NDS_glLoadMatrix4x4,
							NDS_glLoadMatrix4x3,
							NDS_glStoreMatrix,
							NDS_glRestoreMatrix,
							NDS_glPushMatrix,
							NDS_glPopMatrix,
							NDS_glTranslate,
							NDS_glScale,
							NDS_glMultMatrix3x3,
							NDS_glMultMatrix4x3,
							NDS_glMultMatrix4x4,
							NDS_glBegin,
							NDS_glEnd,
							NDS_glColor3b,
							NDS_glVertex16b,
							NDS_glVertex10b,
							NDS_glVertex3_cord,
							NDS_glVertex_rel,
							NDS_glSwapScreen,
							NDS_glGetNumPolys,
							NDS_glGetNumVertex,
							NDS_glFlush,
							NDS_glPolygonAttrib,
							NDS_glMaterial0,
							NDS_glMaterial1,
							NDS_glShininess,
							NDS_glTexImage,
							NDS_glTexPalette,
							NDS_glTexCoord,
							NDS_glLightDirection,
							NDS_glLightColor,
							NDS_glAlphaFunc,
							NDS_glControl,
							NDS_glNormal,
							NDS_glCallList,

							NDS_glGetClipMatrix,
							NDS_glGetDirectionalMatrix,
							NDS_glGetLine,

							NDS_glGetMatrix,
							NDS_glGetLightDirection,
							NDS_glGetLightColor,

							NDS_glBoxTest,
							NDS_glPosTest,
							NDS_glVecTest,
							NDS_glGetPosRes,
							NDS_glGetVecRes
};

