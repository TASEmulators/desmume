/*	Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com 

	Copyright (C) 2008-2009 DeSmuME team

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef _GFX3D_H_
#define _GFX3D_H_

#include "types.h"
#include <iosfwd>

//produce a 32bpp color from a DS RGB16
#define RGB16TO32(col,alpha) (((alpha)<<24) | ((((col) & 0x7C00)>>7)<<16) | ((((col) & 0x3E0)>>2)<<8) | (((col) & 0x1F)<<3))

//produce a 32bpp color from a ds RGB15 plus an 8bit alpha, using a table
#define RGB15TO32(col,alpha8) ( ((alpha8)<<24) | color_15bit_to_24bit[col&0x7FFF] )

//produce a 24bpp color from a ds RGB15, using a table
#define RGB15TO24_REVERSE(col) ( color_15bit_to_24bit_reverse[col&0x7FFF] )

//produce a 16bpp color from a ds RGB15, using a table
#define RGB15TO16_REVERSE(col) ( color_15bit_to_16bit_reverse[col&0x7FFF] )

//produce a 32bpp color from a ds RGB15 plus an 8bit alpha, not using a table (but using other tables)
#define RGB15TO32_DIRECT(col,alpha8) ( ((alpha8)<<24) | (material_5bit_to_8bit[((col)>>10)&0x1F]<<16) | (material_5bit_to_8bit[((col)>>5)&0x1F]<<8) | material_5bit_to_8bit[(col)&0x1F] )

//produce a 15bpp color from individual 5bit components
#define R5G5B5TORGB15(r,g,b) ((r)|((g)<<5)|((b)<<10))


#define TEXMODE_NONE 0
#define TEXMODE_A3I5 1
#define TEXMODE_I2 2
#define TEXMODE_I4 3
#define TEXMODE_I8 4
#define TEXMODE_4X4 5
#define TEXMODE_A5I3 6
#define TEXMODE_16BPP 7

void gfx3d_init();
void gfx3d_reset();

struct POLY {
	int type; //tri or quad
	u16 vertIndexes[4]; //up to four verts can be referenced by this poly
	u32 polyAttr, texParam, texPalette; //the hardware rendering params
//	int projIndex; //the index into the projlist that this poly uses
	u32 pad;
	u32 viewport;

	bool isTranslucent()
	{
		//alpha != 31 -> translucent
		if((polyAttr&0x001F0000) != 0x001F0000) 
			return true;
		int texFormat = (texParam>>26)&7;

		//a5i3 or a3i5 -> translucent
		if(texFormat==1 || texFormat==6) 
			return true;
		
		return false;
	}

	int getAlpha() { return (polyAttr>>16)&0x1F; }
};

#define POLYLIST_SIZE 100000
//#define POLYLIST_SIZE 2048
struct POLYLIST {
	POLY list[POLYLIST_SIZE];
	int count;
};

struct VERT {
	union {
		float coord[4];
		struct {
			float x,y,z,w;
		};
	};
	union {
		float texcoord[2];
		struct {
			float u,v;
		};
	};
	u8 color[3];
	float fcolor[3];
	void color_to_float() {
		fcolor[0] = color[0];
		fcolor[1] = color[1];
		fcolor[2] = color[2];
	}
};

#define VERTLIST_SIZE 400000
//#define VERTLIST_SIZE 10000
struct VERTLIST {
	VERT list[VERTLIST_SIZE];
	int count;
};


struct VIEWPORT {
	int x, y, width, height;
	void decode(u32 v);
};

//used to communicate state to the renderer
struct GFX3D
{
	GFX3D()
		: enableTexturing(true)
		, enableAlphaTest(true)
		, enableAlphaBlending(true)
		, enableAntialiasing(false)
		, enableEdgeMarking(false)
		, shading(TOON)
		, polylist(0)
		, vertlist(0)
		, alphaTestRef(0)
		, clearDepth(1)
		, clearColor(0)
		, frameCtr(0)
		, frameCtrRaw(0)
	{
		fogColor[0] = fogColor[1] = fogColor[2] = fogColor[3] = 0;
		fogOffset = 0;
	}
	BOOL enableTexturing, enableAlphaTest, enableAlphaBlending, enableAntialiasing, enableEdgeMarking;

	static const u32 TOON = 0;
	static const u32 HIGHLIGHT = 1;
	u32 shading;

	POLYLIST* polylist;
	VERTLIST* vertlist;
	int indexlist[POLYLIST_SIZE];

	BOOL wbuffer, sortmode;

	u8 alphaTestRef;

	u32 clearDepth;
	u32 clearColor;
	float fogColor[4];
	float fogOffset;

	//ticks every time flush() is called
	int frameCtr;

	//you can use this to track how many real frames passed, for comparing to frameCtr;
	int frameCtrRaw;

	u32 rgbToonTable[32];
};
extern GFX3D gfx3d;

//---------------------

extern CACHE_ALIGN u32 color_15bit_to_24bit[32768];
extern CACHE_ALIGN u32 color_15bit_to_24bit_reverse[32768];
extern CACHE_ALIGN u16 color_15bit_to_16bit_reverse[32768];
extern CACHE_ALIGN u8 mixTable555[32][32][32];
extern CACHE_ALIGN const int material_5bit_to_31bit[32];
extern CACHE_ALIGN const u8 material_5bit_to_8bit[32];
extern CACHE_ALIGN const u8 material_3bit_to_8bit[8];
extern CACHE_ALIGN const u8 alpha_5bit_to_4bit[32];

//GE commands:
void gfx3d_glViewPort(unsigned long v);
void gfx3d_glClearColor(unsigned long v);
void gfx3d_glFogColor(unsigned long v);
void gfx3d_glFogOffset (unsigned long v);
void gfx3d_glClearDepth(unsigned long v);
void gfx3d_glMatrixMode(unsigned long v);
void gfx3d_glLoadIdentity();
BOOL gfx3d_glLoadMatrix4x4(signed long v);
BOOL gfx3d_glLoadMatrix4x3(signed long v);
void gfx3d_glStoreMatrix(unsigned long v);
void gfx3d_glRestoreMatrix(unsigned long v);
void gfx3d_glPushMatrix(void);
void gfx3d_glPopMatrix(signed long i);
BOOL gfx3d_glTranslate(signed long v);
BOOL gfx3d_glScale(signed long v);
BOOL gfx3d_glMultMatrix3x3(signed long v);
BOOL gfx3d_glMultMatrix4x3(signed long v);
BOOL gfx3d_glMultMatrix4x4(signed long v);
void gfx3d_glBegin(unsigned long v);
void gfx3d_glEnd(void);
void gfx3d_glColor3b(unsigned long v);
BOOL gfx3d_glVertex16b(unsigned int v);
void gfx3d_glVertex10b(unsigned long v);
void gfx3d_glVertex3_cord(unsigned int one, unsigned int two, unsigned int v);
void gfx3d_glVertex_rel(unsigned long v);
void gfx3d_glSwapScreen(unsigned int screen);
int gfx3d_GetNumPolys();
int gfx3d_GetNumVertex();
void gfx3d_glPolygonAttrib (unsigned long val);
void gfx3d_glMaterial0(unsigned long val);
void gfx3d_glMaterial1(unsigned long val);
BOOL gfx3d_glShininess (unsigned long val);
void gfx3d_UpdateToonTable(u8 offset, u16 val);
void gfx3d_UpdateToonTable(u8 offset, u32 val);
void gfx3d_glTexImage(unsigned long val);
void gfx3d_glTexPalette(unsigned long val);
void gfx3d_glTexCoord(unsigned long val);
void gfx3d_glNormal(unsigned long v);
signed long gfx3d_GetClipMatrix (unsigned int index);
signed long gfx3d_GetDirectionalMatrix (unsigned int index);
void gfx3d_glLightDirection (unsigned long v);
void gfx3d_glLightColor (unsigned long v);
void gfx3d_glAlphaFunc(unsigned long v);
BOOL gfx3d_glBoxTest(unsigned long v);
BOOL gfx3d_glPosTest(unsigned long v);
void gfx3d_glVecTest(unsigned long v);
unsigned int gfx3d_glGetPosRes(unsigned int index);
unsigned short gfx3d_glGetVecRes(unsigned int index);
void gfx3d_glFlush(unsigned long v);
void gfx3d_VBlankSignal();
void gfx3d_VBlankEndSignal(bool skipFrame);
void gfx3d_Control(unsigned long v);
u32 gfx3d_GetGXstatus();
void gfx3d_sendCommandToFIFO(u32 val);
void gfx3d_sendCommand(u32 cmd, u32 param);

//other misc stuff
void gfx3d_glGetMatrix(unsigned int mode, int index, float* dest);
void gfx3d_glGetLightDirection(unsigned int index, unsigned int* dest);
void gfx3d_glGetLightColor(unsigned int index, unsigned int* dest);

struct SFORMAT;
extern SFORMAT SF_GFX3D[];
void gfx3d_savestate(std::ostream* os);
bool gfx3d_loadstate(std::istream* is);

#endif
