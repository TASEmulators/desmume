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

#include <iosfwd>
#include <ostream>
#include <istream>
#include "types.h"
#include "emufile.h"


//geometry engine command numbers
#define GFX3D_NOP 0x00
#define GFX3D_MTX_MODE 0x10
#define GFX3D_MTX_PUSH 0x11
#define GFX3D_MTX_POP 0x12
#define GFX3D_MTX_STORE 0x13
#define GFX3D_MTX_RESTORE 0x14
#define GFX3D_MTX_IDENTITY 0x15
#define GFX3D_MTX_LOAD_4x4 0x16
#define GFX3D_MTX_LOAD_4x3 0x17
#define GFX3D_MTX_MULT_4x4 0x18
#define GFX3D_MTX_MULT_4x3 0x19
#define GFX3D_MTX_MULT_3x3 0x1A
#define GFX3D_MTX_SCALE 0x1B
#define GFX3D_MTX_TRANS 0x1C
#define GFX3D_COLOR 0x20
#define GFX3D_NORMAL 0x21
#define GFX3D_TEXCOORD 0x22
#define GFX3D_VTX_16 0x23
#define GFX3D_VTX_10 0x24
#define GFX3D_XY 0x25
#define GFX3D_XZ 0x26
#define GFX3D_YZ 0x27
#define GFX3D_DIFF 0x28
#define GFX3D_POLYGON_ATTR 0x29
#define GFX3D_TEXIMAGE_PARAM 0x2A
#define GFX3D_PLTT_BASE 0x2B
#define GFX3D_DIF_AMB 0x30
#define GFX3D_SPE_EMI 0x31
#define GFX3D_LIGHT_VECTOR 0x32
#define GFX3D_LIGHT_COLOR 0x33
#define GFX3D_SHININESS 0x34
#define GFX3D_BEGIN_VTXS 0x40
#define GFX3D_END_VTXS 0x41
#define GFX3D_SWAP_BUFFERS 0x50
#define GFX3D_VIEWPORT 0x60
#define GFX3D_BOX_TEST 0x70
#define GFX3D_POS_TEST 0x71
#define GFX3D_VEC_TEST 0x72
#define GFX3D_NOP_NOARG_HACK 0xDD
		
//produce a 32bpp color from a DS RGB16
#define RGB16TO32(col,alpha) (((alpha)<<24) | ((((col) & 0x7C00)>>7)<<16) | ((((col) & 0x3E0)>>2)<<8) | (((col) & 0x1F)<<3))

//produce a 32bpp color from a ds RGB15 plus an 8bit alpha, using a table
#define RGB15TO32(col,alpha8) ( ((alpha8)<<24) | color_15bit_to_24bit[col&0x7FFF] )

//produce a 5555 32bit color from a ds RGB15 plus an 5bit alpha
#define RGB15TO5555(col,alpha5) (((alpha5)<<24) | ((((col) & 0x7C00)>>10)<<16) | ((((col) & 0x3E0)>>5)<<8) | (((col) & 0x1F)))

//produce a 6665 32bit color from a ds RGB15 plus an 5bit alpha
inline u32 RGB15TO6665(u16 col, u8 alpha5)
{
	u32 ret = alpha5<<24;
	u16 r = (col&0x1F)>>0;
	u16 g = (col&0x3E0)>>5;
	u16 b = (col&0x7C00)>>10;
	if(r) ret |= ((r<<1)+1);
	if(g) ret |= ((g<<1)+1)<<8;
	if(b) ret |= ((b<<1)+1)<<16;
	return ret;
}

//produce a 24bpp color from a ds RGB15, using a table
#define RGB15TO24_REVERSE(col) ( color_15bit_to_24bit_reverse[col&0x7FFF] )

//produce a 16bpp color from a ds RGB15, using a table
#define RGB15TO16_REVERSE(col) ( color_15bit_to_16bit_reverse[col&0x7FFF] )

//produce a 32bpp color from a ds RGB15 plus an 8bit alpha, not using a table (but using other tables)
#define RGB15TO32_DIRECT(col,alpha8) ( ((alpha8)<<24) | (material_5bit_to_8bit[((col)>>10)&0x1F]<<16) | (material_5bit_to_8bit[((col)>>5)&0x1F]<<8) | material_5bit_to_8bit[(col)&0x1F] )

//produce a 15bpp color from individual 5bit components
#define R5G5B5TORGB15(r,g,b) ((r)|((g)<<5)|((b)<<10))

//produce a 16bpp color from individual 5bit components
#define R6G6B6TORGB15(r,g,b) ((r>>1)|((g&0x3E)<<4)|((b&0x3E)<<9))

#define GFX3D_5TO6(x) ((x)?(((x)<<1)+1):0)

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

#define OSWRITE(x) os->fwrite((char*)&(x),sizeof((x)));
#define OSREAD(x) is->fread((char*)&(x),sizeof((x)));

struct POLY {
	int type; //tri or quad
	u16 vertIndexes[4]; //up to four verts can be referenced by this poly
	u32 polyAttr, texParam, texPalette; //the hardware rendering params
	u32 viewport;
	float miny, maxy;

	void setVertIndexes(int a, int b, int c, int d=-1)
	{
		vertIndexes[0] = a;
		vertIndexes[1] = b;
		vertIndexes[2] = c;
		if(d != -1) { vertIndexes[3] = d; type = 4; }
		else type = 3;
	}

	bool isTranslucent()
	{
		//alpha != 31 -> translucent
		//except for alpha 0 which is wireframe (unless it has a translucent tex)
		if((polyAttr&0x001F0000) != 0x001F0000 && (polyAttr&0x001F0000) != 0)
			return true;
		int texFormat = (texParam>>26)&7;

		//a5i3 or a3i5 -> translucent
		if(texFormat==1 || texFormat==6) 
			return true;
		
		return false;
	}

	int getAlpha() { return (polyAttr>>16)&0x1F; }

	void save(EMUFILE* os)
	{
		OSWRITE(type); 
		OSWRITE(vertIndexes[0]); OSWRITE(vertIndexes[1]); OSWRITE(vertIndexes[2]); OSWRITE(vertIndexes[3]);
		OSWRITE(polyAttr); OSWRITE(texParam); OSWRITE(texPalette);
		OSWRITE(viewport);
		OSWRITE(miny);
		OSWRITE(maxy);
	}

	void load(EMUFILE* is)
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
	void set_coord(float x, float y, float z, float w) { 
		this->x = x; 
		this->y = y; 
		this->z = z; 
		this->w = w; 
	}
	void set_coord(float* coords) { 
		x = coords[0];
		y = coords[1];
		z = coords[2];
		w = coords[3];
	}
	u8 color[3];
	float fcolor[3];
	void color_to_float() {
		fcolor[0] = color[0];
		fcolor[1] = color[1];
		fcolor[2] = color[2];
	}
	void save(EMUFILE* os)
	{
		OSWRITE(x); OSWRITE(y); OSWRITE(z); OSWRITE(w);
		OSWRITE(u); OSWRITE(v);
		OSWRITE(color[0]); OSWRITE(color[1]); OSWRITE(color[2]);
		OSWRITE(fcolor[0]); OSWRITE(fcolor[1]); OSWRITE(fcolor[2]);
	}
	void load(EMUFILE* is)
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

//ok, imagine the plane that cuts diagonally across a cube such that it clips
//out to be a hexagon. within that plane, draw a quad such that it cuts off
//four corners of the hexagon, and you will observe a decagon
#define MAX_CLIPPED_VERTS 10

class GFX3D_Clipper
{
public:
	
	struct TClippedPoly
	{
		int type; //otherwise known as "count" of verts
		POLY* poly;
		VERT clipVerts[MAX_CLIPPED_VERTS];
	};

	//the entry point for poly clipping
	void clipPoly(POLY* poly, VERT** verts);

	//the output of clipping operations goes into here.
	//be sure you init it before clipping!
	TClippedPoly *clippedPolys;
	int clippedPolyCounter;

private:
	TClippedPoly tempClippedPoly;
	TClippedPoly outClippedPoly;
	FORCEINLINE void clipSegmentVsPlane(VERT** verts, const int coord, int which);
	FORCEINLINE void clipPolyVsPlane(const int coord, int which);
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
		, enableFog(false)
		, enableFogAlphaOnly(false)
		, fogShift(0)
		, shading(TOON)
		, polylist(0)
		, vertlist(0)
		, alphaTestRef(0)
		, clearDepth(1)
		, clearColor(0)
		, fogColor(0)
		, fogOffset(0)
		, frameCtr(0)
		, frameCtrRaw(0)
	{
		for(int i=0;i<ARRAY_SIZE(u16ToonTable);i++)
			u16ToonTable[i] = 0;
	}
	BOOL enableTexturing, enableAlphaTest, enableAlphaBlending, 
		enableAntialiasing, enableEdgeMarking, enableClearImage, enableFog, enableFogAlphaOnly;

	u32 fogShift;

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
	#include "PACKED.h"
	struct {
		u32 fogColor;
		u32 pad[3]; //for savestate compatibility as of 26-jul-09
	};
	#include "PACKED_END.h"
	u32 fogOffset;

	//ticks every time flush() is called
	int frameCtr;

	//you can use this to track how many real frames passed, for comparing to frameCtr;
	int frameCtrRaw;

	u16 u16ToonTable[32];
};
extern GFX3D gfx3d;

//---------------------

extern CACHE_ALIGN u32 color_15bit_to_24bit[32768];
extern CACHE_ALIGN u32 color_15bit_to_24bit_reverse[32768];
extern CACHE_ALIGN u16 color_15bit_to_16bit_reverse[32768];
extern CACHE_ALIGN u8 mixTable555[32][32][32];
extern CACHE_ALIGN const int material_5bit_to_31bit[32];
extern CACHE_ALIGN const u8 material_5bit_to_8bit[32];
extern CACHE_ALIGN const u8 material_3bit_to_5bit[8];
extern CACHE_ALIGN const u8 material_3bit_to_6bit[8];
extern CACHE_ALIGN const u8 material_3bit_to_8bit[8];

//these contain the 3d framebuffer converted into the most useful format
//they are stored here instead of in the renderers in order to consolidate the buffers
extern CACHE_ALIGN u8 gfx3d_convertedScreen[256*192*4];
extern CACHE_ALIGN u8 gfx3d_convertedAlpha[256*192*2]; //see cpp for explanation of illogical *2

extern BOOL isSwapBuffers;

int _hack_getMatrixStackLevel(int);

void gfx3d_glFlush(u32 v);
// end GE commands

void gfx3d_glClearColor(u32 v);
void gfx3d_glFogColor(u32 v);
void gfx3d_glFogOffset (u32 v);
void gfx3d_glClearDepth(u32 v);
void gfx3d_glSwapScreen(u32 screen);
int gfx3d_GetNumPolys();
int gfx3d_GetNumVertex();
void gfx3d_UpdateToonTable(u8 offset, u16 val);
void gfx3d_UpdateToonTable(u8 offset, u32 val);
s32 gfx3d_GetClipMatrix (u32 index);
s32 gfx3d_GetDirectionalMatrix (u32 index);
void gfx3d_glAlphaFunc(u32 v);
u32 gfx3d_glGetPosRes(u32 index);
u16 gfx3d_glGetVecRes(u32 index);
void gfx3d_VBlankSignal();
void gfx3d_VBlankEndSignal(bool skipFrame);
void gfx3d_Control(u32 v);
void gfx3d_execute3D();
void gfx3d_sendCommandToFIFO(u32 val);
void gfx3d_sendCommand(u32 cmd, u32 param);

//other misc stuff
void gfx3d_glGetMatrix(u32 mode, int index, float* dest);
void gfx3d_glGetLightDirection(u32 index, u32* dest);
void gfx3d_glGetLightColor(u32 index, u32* dest);

void gfx3d_GetLineData(int line, u8** dst);
void gfx3d_GetLineData15bpp(int line, u16** dst);

struct SFORMAT;
extern SFORMAT SF_GFX3D[];
void gfx3d_savestate(EMUFILE* os);
bool gfx3d_loadstate(EMUFILE* is, int size);

void gfx3d_ClearStack();

#endif //_GFX3D_H_
