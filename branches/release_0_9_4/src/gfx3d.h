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
#include <ostream>
#include <istream>

//produce a 32bpp color from a DS RGB16
#define RGB16TO32(col,alpha) (((alpha)<<24) | ((((col) & 0x7C00)>>7)<<16) | ((((col) & 0x3E0)>>2)<<8) | (((col) & 0x1F)<<3))

//produce a 32bpp color from a ds RGB15 plus an 8bit alpha, using a table
#define RGB15TO32(col,alpha8) ( ((alpha8)<<24) | color_15bit_to_24bit[col&0x7FFF] )

//produce a 5555 32bit color from a ds RGB15 plus an 5bit alpha
#define RGB15TO5555(col,alpha5) (((alpha5)<<24) | ((((col) & 0x7C00)>>10)<<16) | ((((col) & 0x3E0)>>5)<<8) | (((col) & 0x1F)))

//produce a 24bpp color from a ds RGB15, using a table
#define RGB15TO24_REVERSE(col) ( color_15bit_to_24bit_reverse[col&0x7FFF] )

//produce a 16bpp color from a ds RGB15, using a table
#define RGB15TO16_REVERSE(col) ( color_15bit_to_16bit_reverse[col&0x7FFF] )

//produce a 32bpp color from a ds RGB15 plus an 8bit alpha, not using a table (but using other tables)
#define RGB15TO32_DIRECT(col,alpha8) ( ((alpha8)<<24) | (material_5bit_to_8bit[((col)>>10)&0x1F]<<16) | (material_5bit_to_8bit[((col)>>5)&0x1F]<<8) | material_5bit_to_8bit[(col)&0x1F] )

//produce a 15bpp color from individual 5bit components
#define R5G5B5TORGB15(r,g,b) ((r)|((g)<<5)|((b)<<10))

inline u32 gfx3d_extendDepth_15_to_24(u32 depth)
{
	//formula from http://nocash.emubase.de/gbatek.htm#ds3drearplane
	//return (depth*0x200)+((depth+1)>>15)*0x01FF;
	//I think this might be slightly faster
	if(depth==0x7FFF) return 0x00FFFFFF;
	else return depth<<9;
}

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

#define OSWRITE(x) os->write((char*)&(x),sizeof((x)));
#define OSREAD(x) is->read((char*)&(x),sizeof((x)));

struct POLY {
	int type; //tri or quad
	u16 vertIndexes[4]; //up to four verts can be referenced by this poly
	u32 polyAttr, texParam, texPalette; //the hardware rendering params
	u32 viewport;
	float miny, maxy;

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

	void save(std::ostream* os)
	{
		OSWRITE(type); 
		OSWRITE(vertIndexes[0]); OSWRITE(vertIndexes[1]); OSWRITE(vertIndexes[2]); OSWRITE(vertIndexes[3]);
		OSWRITE(polyAttr); OSWRITE(texParam); OSWRITE(texPalette);
		OSWRITE(viewport);
		OSWRITE(miny);
		OSWRITE(maxy);
	}

	void load(std::istream* is)
	{
		OSREAD(type); 
		OSREAD(vertIndexes[0]); OSREAD(vertIndexes[1]); OSREAD(vertIndexes[2]); OSREAD(vertIndexes[3]);
		OSREAD(polyAttr); OSREAD(texParam); OSREAD(texPalette);
		OSREAD(viewport);
		OSREAD(miny);
		OSREAD(maxy);
	}
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
	void save(std::ostream* os)
	{
		OSWRITE(x); OSWRITE(y); OSWRITE(z); OSWRITE(w);
		OSWRITE(u); OSWRITE(v);
		OSWRITE(color[0]); OSWRITE(color[1]); OSWRITE(color[2]);
		OSWRITE(fcolor[0]); OSWRITE(fcolor[1]); OSWRITE(fcolor[2]);
	}
	void load(std::istream* is)
	{
		OSREAD(x); OSREAD(y); OSREAD(z); OSREAD(w);
		OSREAD(u); OSREAD(v);
		OSREAD(color[0]); OSREAD(color[1]); OSREAD(color[2]);
		OSREAD(fcolor[0]); OSREAD(fcolor[1]); OSREAD(fcolor[2]);
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
		, enableClearImage(false)
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
	BOOL enableTexturing, enableAlphaTest, enableAlphaBlending, 
		enableAntialiasing, enableEdgeMarking, enableClearImage;

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

	u16 u16ToonTable[32];
};
extern GFX3D gfx3d;

//---------------------

extern CACHE_ALIGN const u16 alpha_lookup[32];
extern CACHE_ALIGN u32 color_15bit_to_24bit[32768];
extern CACHE_ALIGN u32 color_15bit_to_24bit_reverse[32768];
extern CACHE_ALIGN u16 color_15bit_to_16bit_reverse[32768];
extern CACHE_ALIGN u8 mixTable555[32][32][32];
extern CACHE_ALIGN const int material_5bit_to_31bit[32];
extern CACHE_ALIGN const u8 material_5bit_to_8bit[32];
extern CACHE_ALIGN const u8 material_3bit_to_5bit[8];
extern CACHE_ALIGN const u8 material_3bit_to_8bit[8];
extern CACHE_ALIGN const u8 alpha_5bit_to_4bit[32];

//these contain the 3d framebuffer converted into the most useful format
//they are stored here instead of in the renderers in order to consolidate the buffers
extern CACHE_ALIGN u16 gfx3d_convertedScreen[256*192];
extern CACHE_ALIGN u8 gfx3d_convertedAlpha[256*192*2]; //see cpp for explanation of illogical *2

//GE commands:
void gfx3d_glViewPort(u32 v);
void gfx3d_glClearColor(u32 v);
void gfx3d_glFogColor(u32 v);
void gfx3d_glFogOffset (u32 v);
void gfx3d_glClearDepth(u32 v);
void gfx3d_glMatrixMode(u32 v);
void gfx3d_glLoadIdentity();
BOOL gfx3d_glLoadMatrix4x4(s32 v);
BOOL gfx3d_glLoadMatrix4x3(s32 v);
void gfx3d_glStoreMatrix(u32 v);
void gfx3d_glRestoreMatrix(u32 v);
void gfx3d_glPushMatrix(void);
void gfx3d_glPopMatrix(s32 i);
BOOL gfx3d_glTranslate(s32 v);
BOOL gfx3d_glScale(s32 v);
BOOL gfx3d_glMultMatrix3x3(s32 v);
BOOL gfx3d_glMultMatrix4x3(s32 v);
BOOL gfx3d_glMultMatrix4x4(s32 v);
void gfx3d_glBegin(u32 v);
void gfx3d_glEnd(void);
void gfx3d_glColor3b(u32 v);
BOOL gfx3d_glVertex16b(u32 v);
void gfx3d_glVertex10b(u32 v);
void gfx3d_glVertex3_cord(u32 one, u32 two, u32 v);
void gfx3d_glVertex_rel(u32 v);
void gfx3d_glSwapScreen(u32 screen);
int gfx3d_GetNumPolys();
int gfx3d_GetNumVertex();
void gfx3d_glPolygonAttrib (u32 val);
void gfx3d_glMaterial0(u32 val);
void gfx3d_glMaterial1(u32 val);
BOOL gfx3d_glShininess (u32 val);
void gfx3d_UpdateToonTable(u8 offset, u16 val);
void gfx3d_UpdateToonTable(u8 offset, u32 val);
void gfx3d_glTexImage(u32 val);
void gfx3d_glTexPalette(u32 val);
void gfx3d_glTexCoord(u32 val);
void gfx3d_glNormal(u32 v);
s32 gfx3d_GetClipMatrix (u32 index);
s32 gfx3d_GetDirectionalMatrix (u32 index);
void gfx3d_glLightDirection (u32 v);
void gfx3d_glLightColor (u32 v);
void gfx3d_glAlphaFunc(u32 v);
BOOL gfx3d_glBoxTest(u32 v);
BOOL gfx3d_glPosTest(u32 v);
void gfx3d_glVecTest(u32 v);
u32 gfx3d_glGetPosRes(u32 index);
u16 gfx3d_glGetVecRes(u32 index);
void gfx3d_glFlush(u32 v);
void gfx3d_VBlankSignal();
void gfx3d_VBlankEndSignal(bool skipFrame);
void gfx3d_Control(u32 v);
u32 gfx3d_GetGXstatus();
void gfx3d_sendCommandToFIFO(u32 val);
void gfx3d_sendCommand(u32 cmd, u32 param);

//other misc stuff
void gfx3d_glGetMatrix(u32 mode, int index, float* dest);
void gfx3d_glGetLightDirection(u32 index, u32* dest);
void gfx3d_glGetLightColor(u32 index, u32* dest);

void gfx3d_GetLineData(int line, u16** dst, u8** dstAlpha);

struct SFORMAT;
extern SFORMAT SF_GFX3D[];
void gfx3d_savestate(std::ostream* os);
bool gfx3d_loadstate(std::istream* is, int size);

#endif
