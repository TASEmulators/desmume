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

//This file implements the geometry engine hardware component.
//This handles almost all of the work of 3d rendering, leaving the renderer
//plugin responsible only for drawing primitives.

#include <algorithm>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "armcpu.h"
#include "debug.h"
#include "gfx3d.h"
#include "matrix.h"
#include "bits.h"
#include "MMU.h"
#include "render3D.h"
#include "mem.h"
#include "types.h"
#include "saves.h"
#include "NDSSystem.h"
#include "readwrite.h"
#include "FIFO.h"

/*
thoughts on flush timing:
I think a flush is supposed to queue up and wait to happen during vblank sometime.
But, we have some games that continue to do work after a flush but before a vblank.
Since our timing is bad anyway, and we're not sure when the flush is really supposed to happen,
then this leaves us in a bad situation.
What makes it worse is that if flush is supposed to be deferred, then we have to queue these
errant geometry commands. That would require a better gxfifo we have now, and some mechanism to block
while the geometry engine is stalled (which doesnt exist).
Since these errant games are nevertheless using flush command to represent the end of a frame, we deem this
a good time to execute an actual flush.
I think we originally didnt do this because we found some game that it glitched, but that may have been
resolved since then by deferring actual rendering to the next vcount=0 (giving textures enough time to upload).
But since we're not sure how we'll eventually want this, I am leaving it sort of reconfigurable, doing all the work
in this function: */
static void gfx3d_doFlush();


using std::max;
using std::min;

GFX3D gfx3d;

//tables that are provided to anyone
CACHE_ALIGN u32 color_15bit_to_24bit_reverse[32768];
CACHE_ALIGN u32 color_15bit_to_24bit[32768];
CACHE_ALIGN u16 color_15bit_to_16bit_reverse[32768];
CACHE_ALIGN u8 mixTable555[32][32][32];

//is this a crazy idea? this table spreads 5 bits evenly over 31 from exactly 0 to INT_MAX
CACHE_ALIGN const int material_5bit_to_31bit[] = {
	0x00000000, 0x04210842, 0x08421084, 0x0C6318C6,
	0x10842108, 0x14A5294A, 0x18C6318C, 0x1CE739CE,
	0x21084210, 0x25294A52, 0x294A5294, 0x2D6B5AD6,
	0x318C6318, 0x35AD6B5A, 0x39CE739C, 0x3DEF7BDE,
	0x42108421, 0x46318C63, 0x4A5294A5, 0x4E739CE7,
	0x5294A529, 0x56B5AD6B, 0x5AD6B5AD, 0x5EF7BDEF,
	0x6318C631, 0x6739CE73, 0x6B5AD6B5, 0x6F7BDEF7,
	0x739CE739, 0x77BDEF7B, 0x7BDEF7BD, 0x7FFFFFFF
};

CACHE_ALIGN const u8 material_5bit_to_6bit[] = {
	0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E,
	0x10, 0x12, 0x14, 0x16, 0x19, 0x1A, 0x1C, 0x1E,
	0x21, 0x23, 0x25, 0x27, 0x29, 0x2B, 0x2D, 0x2F,
	0x31, 0x33, 0x35, 0x37, 0x39, 0x3B, 0x3D, 0x3F
};

CACHE_ALIGN const u8 material_5bit_to_8bit[] = {
	0x00, 0x08, 0x10, 0x18, 0x21, 0x29, 0x31, 0x39,
	0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD,
	0xC6, 0xCE, 0xD6, 0xDE, 0xE7, 0xEF, 0xF7, 0xFF
};

CACHE_ALIGN const u8 material_3bit_to_8bit[] = {
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF
};

//maybe not very precise
CACHE_ALIGN const u8 material_3bit_to_5bit[] = {
	0, 4, 8, 13, 17, 22, 26, 31
};

CACHE_ALIGN const u8 alpha_5bit_to_4bit[] = {
	0x00, 0x00,
	0x01, 0x01,
	0x02, 0x02,
	0x03, 0x03,
	0x04, 0x04,
	0x05, 0x05,
	0x06, 0x06,
	0x07, 0x07,
	0x08, 0x08,
	0x09, 0x09,
	0x0A, 0x0A,
	0x0B, 0x0B,
	0x0C, 0x0C,
	0x0D, 0x0D,
	0x0E, 0x0E,
	0x10, 0x10
};

CACHE_ALIGN const u16 alpha_lookup[] = {
	0x0000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,
	0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,
	0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,
	0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000};


//private acceleration tables
static float float16table[65536];
static float float10Table[1024];
static float float10RelTable[1024];
static float normalTable[1024];

#define fix2float(v)    (((float)((s32)(v))) / (float)(1<<12))
#define fix10_2float(v) (((float)((s32)(v))) / (float)(1<<9))

CACHE_ALIGN u16 gfx3d_convertedScreen[256*192];

//this extra *2 is a HACK to salvage some savestates. remove me when the savestate format changes.
CACHE_ALIGN u8 gfx3d_convertedAlpha[256*192*2];

// Matrix stack handling
static CACHE_ALIGN MatrixStack	mtxStack[4] = {
	MatrixStack(1), // Projection stack
	MatrixStack(31), // Coordinate stack
	MatrixStack(31), // Directional stack
	MatrixStack(1), // Texture stack
};

static CACHE_ALIGN float		mtxCurrent [4][16];
static CACHE_ALIGN float		mtxTemporal[16];
static u32 mode = 0;

// Indexes for matrix loading/multiplication
static u8 ML4x4ind = 0;
static u8 ML4x3ind = 0;
static u8 MM4x4ind = 0;
static u8 MM4x3ind = 0;
static u8 MM3x3ind = 0;

// Data for vertex submission
static CACHE_ALIGN float	coord[4] = {0.0, 0.0, 0.0, 0.0};
static char		coordind = 0;
static u32 vtxFormat;
static BOOL inBegin = FALSE;

// Data for basic transforms
static CACHE_ALIGN float	trans[4] = {0.0, 0.0, 0.0, 0.0};
static int		transind = 0;
static CACHE_ALIGN float	scale[4] = {0.0, 0.0, 0.0, 0.0};
static int		scaleind = 0;
static u32 viewport;

//various other registers
static float _t=0, _s=0;
static float last_t, last_s;
static u32 clCmd = 0;
static u32 clInd = 0;
#ifdef USE_GEOMETRY_FIFO_EMULATION
static u32 clInd2 = 0;
#endif
static u32 BTind = 0;
static u32 PTind = 0;
//static CACHE_ALIGN float BTcoords[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
static CACHE_ALIGN float PTcoords[4] = {0.0, 0.0, 0.0, 1.0};

//raw ds format poly attributes
static u32 polyAttr=0,textureFormat=0, texturePalette=0, polyAttrPending=0;

//the current vertex color, 5bit values
static int colorRGB[4] = { 31,31,31,31 };

u32 control = 0;

//light state:
static u32 lightColor[4] = {0,0,0,0};
static u32 lightDirection[4] = {0,0,0,0};
//material state:
static u16 dsDiffuse, dsAmbient, dsSpecular, dsEmission;
// Shininess
static float shininessTable[128] = {0};
static int shininessInd = 0;


//-----------cached things:
//these dont need to go into the savestate. they can be regenerated from HW registers
//from polygonattr:
static unsigned int cullingMask=0;
static u8 colorAlpha=0;
static u32 envMode=0;
static u32 lightMask=0;
//other things:
static int texCoordinateTransform = 0;
static CACHE_ALIGN float cacheLightDirection[4][4];
static CACHE_ALIGN float cacheHalfVector[4][4];
//------------------

#define RENDER_FRONT_SURFACE 0x80
#define RENDER_BACK_SURFACE 0X40


//-------------poly and vertex lists and such things
POLYLIST* polylists = NULL;
POLYLIST* polylist = NULL;
VERTLIST* vertlists = NULL;
VERTLIST* vertlist = NULL;

int listTwiddle = 1;
int triStripToggle;

//list-building state
struct tmpVertInfo {
	//the number of verts registered in this list
	int count;
	//indices to the main vert list
	int map[4];
	//indicates that the first poly in a list has been completed
	BOOL first;
} tempVertInfo;


static void twiddleLists() {
	listTwiddle++;
	listTwiddle &= 1;
	polylist = &polylists[listTwiddle];
	vertlist = &vertlists[listTwiddle];
	polylist->count = 0;
	vertlist->count = 0;
}

static BOOL flushPending = FALSE;
static BOOL drawPending = FALSE;
//------------------------------------------------------------

static void makeTables() {

	//produce the color bits of a 24bpp color from a DS RGB15 using bit logic (internal use only)
	#define RGB15TO24_BITLOGIC(col) ( (material_5bit_to_8bit[((col)>>10)&0x1F]<<16) | (material_5bit_to_8bit[((col)>>5)&0x1F]<<8) | material_5bit_to_8bit[(col)&0x1F] )

	for(int i=0;i<32768;i++)
		color_15bit_to_24bit[i] = RGB15TO24_BITLOGIC((u16)i);

	//produce the color bits of a 24bpp color from a DS RGB15 using bit logic (internal use only). RGB are reverse of usual
	#define RGB15TO24_BITLOGIC_REVERSE(col) ( (material_5bit_to_8bit[(col)&0x1F]<<16) | (material_5bit_to_8bit[((col)>>5)&0x1F]<<8) | material_5bit_to_8bit[((col)>>10)&0x1F] )

	for(int i=0;i<32768;i++)
	{
		color_15bit_to_24bit_reverse[i] = RGB15TO24_BITLOGIC_REVERSE((u16)i);
		color_15bit_to_16bit_reverse[i] = (((i & 0x001F) << 11) | (material_5bit_to_6bit[(i & 0x03E0) >> 5] << 5) | ((i & 0x7C00) >> 10));
	}

	for (int i = 0; i < 65536; i++)
		float16table[i] = fix2float((signed short)i);

	for (int i = 0; i < 1024; i++)
		float10Table[i] = ((signed short)(i<<6)) / (float)(1<<12);

	for (int i = 0; i < 1024; i++)
		float10RelTable[i] = ((signed short)(i<<6)) / (float)(1<<18);

	for (int i = 0; i < 1024; i++)
		normalTable[i] = ((signed short)(i<<6)) / (float)(1<<15);

	for(int r=0;r<=31;r++) 
		for(int oldr=0;oldr<=31;oldr++) 
			for(int a=0;a<=31;a++)  {
				int temp = (r*a + oldr*(31-a)) / 31;
				mixTable555[a][r][oldr] = temp;
			}
}

void gfx3d_init()
{
	if(polylists == NULL) { polylists = new POLYLIST[2]; polylist = &polylists[0]; }
	if(vertlists == NULL) { vertlists = new VERTLIST[2]; vertlist = &vertlists[0]; }
	makeTables();
	gfx3d_reset();
}

void gfx3d_reset()
{
	gfx3d = GFX3D();

	drawPending = FALSE;
	flushPending = FALSE;
	listTwiddle = 1;
	twiddleLists();

	MatrixInit (mtxCurrent[0]);
	MatrixInit (mtxCurrent[1]);
	MatrixInit (mtxCurrent[2]);
	MatrixInit (mtxCurrent[3]);
	MatrixInit (mtxTemporal);

	clCmd = 0;
	clInd = 0;
#ifdef USE_GEOMETRY_FIFO_EMULATION
	clInd2 = 0;
#endif

	ML4x4ind = 0;
	ML4x3ind = 0;
	MM4x4ind = 0;
	MM4x3ind = 0;
	MM3x3ind = 0;

	BTind = 0;
	PTind = 0;

	_t=0;
	_s=0;
	last_t = 0;
	last_s = 0;
	viewport = 0xBFFF0000;

	memset(gfx3d_convertedScreen,0,sizeof(gfx3d_convertedScreen));
	memset(gfx3d_convertedAlpha,0,sizeof(gfx3d_convertedAlpha));

	gfx3d.clearDepth = gfx3d_extendDepth_15_to_24(0x7FFF);
	
	GFX_FIFOclear();
}

void gfx3d_glViewPort(u32 v)
{
	viewport = v;
}

void VIEWPORT::decode(u32 v) 
{
	x = (v&0xFF);
	y = std::min(191,(int)(((v>>8)&0xFF)));
	width = (((v>>16)&0xFF)+1)-(v&0xFF);
	height = ((v>>24)+1)-((v>>8)&0xFF);
}


void gfx3d_glClearColor(u32 v)
{
	gfx3d.clearColor = v;

}

void gfx3d_glFogColor(u32 v)
{
	gfx3d.fogColor[0] = ((float)((v    )&0x1F))/31.0f;
	gfx3d.fogColor[1] = ((float)((v>> 5)&0x1F))/31.0f;
	gfx3d.fogColor[2] = ((float)((v>>10)&0x1F))/31.0f;
	gfx3d.fogColor[3] = ((float)((v>>16)&0x1F))/31.0f;
}

void gfx3d_glFogOffset (u32 v)
{
	gfx3d.fogOffset = (float)(v&0xffff);
}

void gfx3d_glClearDepth(u32 v)
{
	v &= 0x7FFF;
	gfx3d.clearDepth = gfx3d_extendDepth_15_to_24(v);
}

void gfx3d_glMatrixMode(u32 v)
{
	mode = (v&3);
}


void gfx3d_glLoadIdentity()
{
	MatrixIdentity (mtxCurrent[mode]);

	if (mode == 2)
		MatrixIdentity (mtxCurrent[1]);
}

BOOL gfx3d_glLoadMatrix4x4(s32 v)
{
	mtxCurrent[mode][ML4x4ind] = fix2float(v);

	++ML4x4ind;
	if(ML4x4ind<16) return FALSE;
	ML4x4ind = 0;

	if (mode == 2)
		MatrixCopy (mtxCurrent[1], mtxCurrent[2]);
	
	return TRUE;
}

BOOL gfx3d_glLoadMatrix4x3(s32 v)
{
	mtxCurrent[mode][ML4x3ind] = fix2float(v);

	ML4x3ind++;
	if((ML4x3ind & 0x03) == 3) ML4x3ind++;
	if(ML4x3ind<16) return FALSE;
	ML4x3ind = 0;

	//fill in the unusued matrix values
	mtxCurrent[mode][3] = mtxCurrent[mode][7] = mtxCurrent[mode][11] = 0;
	mtxCurrent[mode][15] = 1;

	if (mode == 2)
		MatrixCopy (mtxCurrent[1], mtxCurrent[2]);
	return TRUE;
}

void gfx3d_glStoreMatrix(u32 v)
{
	//this command always works on both pos and vector when either pos or pos-vector are the current mtx mode
	short mymode = (mode==1?2:mode);

	//limit height of these stacks.
	//without the mymode==3 namco classics galaxian will try to use pos=1 and overrun the stack, corrupting emu
	if(mymode==0 || mymode==3)
		v = 0;

	if(v==31) v=30; //? what should happen in this case?

	MatrixStackLoadMatrix (&mtxStack[mymode], v&31, mtxCurrent[mymode]);
	if(mymode==2)
		MatrixStackLoadMatrix (&mtxStack[1], v&31, mtxCurrent[1]);
}

void gfx3d_glRestoreMatrix(u32 v)
{
	//this command always works on both pos and vector when either pos or pos-vector are the current mtx mode
	short mymode = (mode==1?2:mode);

	//limit height of these stacks
	//without the mymode==3 namco classics galaxian will try to use pos=1 and overrun the stack, corrupting emu
	if(mymode==0 || mymode==3)
		v = 0;

	if(v==31) v=30; //? what should happen in this case?

	MatrixCopy (mtxCurrent[mymode], MatrixStackGetPos(&mtxStack[mymode], v&31));
	if (mymode == 2)
		MatrixCopy (mtxCurrent[1], MatrixStackGetPos(&mtxStack[1], v&31));
}

void gfx3d_glPushMatrix()
{
	u32 gxstat = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600);
	//this command always works on both pos and vector when either pos or pos-vector are the current mtx mode
	short mymode = (mode==1?2:mode);

	if (mtxStack[mymode].position > mtxStack[mymode].size)
	{
		gxstat |= (1<<15);
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600, gxstat);
		return;
	}

	gxstat &= 0xFFFF00FF;

	MatrixStackPushMatrix(&mtxStack[mymode], mtxCurrent[mymode]);
	if(mymode==2)
		MatrixStackPushMatrix (&mtxStack[1], mtxCurrent[1]);

	gxstat |= ((mtxStack[0].position << 13) | (mtxStack[1].position << 8));
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600, gxstat);
}

void gfx3d_glPopMatrix(s32 i)
{
	u32 gxstat = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600);

	//this command always works on both pos and vector when either pos or pos-vector are the current mtx mode
	short mymode = (mode==1?2:mode);

	/*
	if (i > mtxStack[mymode].position)
	{
		gxstat |= (1<<15);
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600, gxstat);
		return;
	}
	*/
	gxstat &= 0xFFFF00FF;

	MatrixCopy(mtxCurrent[mymode], MatrixStackPopMatrix (&mtxStack[mymode], i));

	if (mymode == 2)
		MatrixCopy(mtxCurrent[1], MatrixStackPopMatrix (&mtxStack[1], i));

	gxstat |= ((mtxStack[0].position << 13) | (mtxStack[1].position << 8));
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600, gxstat);
}

BOOL gfx3d_glTranslate(s32 v)
{
	trans[transind] = fix2float(v);

	++transind;

	if(transind<3) return FALSE;
	transind = 0;

	MatrixTranslate (mtxCurrent[mode], trans);

	if (mode == 2)
		MatrixTranslate (mtxCurrent[1], trans);
	return TRUE;
}

BOOL gfx3d_glScale(s32 v)
{
	short mymode = (mode==2?1:mode);

	scale[scaleind] = fix2float(v);

	++scaleind;

	if(scaleind<3) return FALSE;
	scaleind = 0;

	MatrixScale (mtxCurrent[mymode], scale);

	//note: pos-vector mode should not cause both matrices to scale.
	//the whole purpose is to keep the vector matrix orthogonal
	//so, I am leaving this commented out as an example of what not to do.
	//if (mode == 2)
	//	MatrixScale (mtxCurrent[1], scale);
	return TRUE;
}

BOOL gfx3d_glMultMatrix3x3(s32 v)
{
	mtxTemporal[MM3x3ind] = fix2float(v);

	MM3x3ind++;
	if((MM3x3ind & 0x03) == 3) MM3x3ind++;
	if(MM3x3ind<12) return FALSE;
	MM3x3ind = 0;

	//fill in the unusued matrix values
	mtxTemporal[3] = mtxTemporal[7] = mtxTemporal[11] = 0;
	mtxTemporal[15] = 1;
	mtxTemporal[12] = mtxTemporal[13] = mtxTemporal[14] = 0;

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);

	if (mode == 2)
		MatrixMultiply (mtxCurrent[1], mtxTemporal);

	//does this really need to be done?
	MatrixIdentity (mtxTemporal);
	return TRUE;
}

BOOL gfx3d_glMultMatrix4x3(s32 v)
{
	mtxTemporal[MM4x3ind] = fix2float(v);

	MM4x3ind++;
	if((MM4x3ind & 0x03) == 3) MM4x3ind++;
	if(MM4x3ind<16) return FALSE;
	MM4x3ind = 0;

	//fill in the unusued matrix values
	mtxTemporal[3] = mtxTemporal[7] = mtxTemporal[11] = 0;
	mtxTemporal[15] = 1;

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);
	if (mode == 2)
		MatrixMultiply (mtxCurrent[1], mtxTemporal);

	//does this really need to be done?
	MatrixIdentity (mtxTemporal);
	return TRUE;
}

BOOL gfx3d_glMultMatrix4x4(s32 v)
{
	mtxTemporal[MM4x4ind] = fix2float(v);

	MM4x4ind++;
	if(MM4x4ind<16) return FALSE;
	MM4x4ind = 0;

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);
	if (mode == 2)
		MatrixMultiply (mtxCurrent[1], mtxTemporal);

	MatrixIdentity (mtxTemporal);
	return TRUE;
}

static void gfx3d_glPolygonAttrib_cache()
{
	// Light enable/disable
	lightMask = (polyAttr&0xF);

	// texture environment
	envMode = (polyAttr&0x30)>>4;

	// back face culling
	cullingMask = (polyAttr>>6)&3;

	// Alpha value, actually not well handled, 0 should be wireframe
	colorRGB[3] = colorAlpha = ((polyAttr>>16)&0x1F);
}

void gfx3d_glBegin(u32 v)
{
	inBegin = TRUE;
	vtxFormat = v&0x03;
	triStripToggle = 0;
	tempVertInfo.count = 0;
	tempVertInfo.first = true;
	polyAttr = polyAttrPending;
	gfx3d_glPolygonAttrib_cache();
}

void gfx3d_glEnd(void)
{
	inBegin = FALSE;
	tempVertInfo.count = 0;
}

void gfx3d_glColor3b(u32 v)
{
	colorRGB[0] = (v&0x1F);
	colorRGB[1] = ((v>>5)&0x1F);
	colorRGB[2] = ((v>>10)&0x1F);
}

static void SUBMITVERTEX(int i, int n) 
{
	polylist->list[polylist->count].vertIndexes[i] = tempVertInfo.map[n];
}


//Submit a vertex to the GE
static void SetVertex()
{
	ALIGN(16) float coordTransformed[4] = { coord[0], coord[1], coord[2], 1 };
	if (texCoordinateTransform == 3)
	{
		last_s =((coord[0]*mtxCurrent[3][0] +
					coord[1]*mtxCurrent[3][4] +
					coord[2]*mtxCurrent[3][8]) + _s * 16.0f) / 16.0f;
		last_t =((coord[0]*mtxCurrent[3][1] +
					coord[1]*mtxCurrent[3][5] +
					coord[2]*mtxCurrent[3][9]) + _t * 16.0f) / 16.0f;
	}

	//refuse to do anything if we have too many verts or polys
	if(vertlist->count >= VERTLIST_SIZE)
		return;
	if(polylist->count >= POLYLIST_SIZE)
		return;

	//apply modelview matrix
	MatrixMultVec4x4 (mtxCurrent[1], coordTransformed);

	//apply projection matrix
	MatrixMultVec4x4 (mtxCurrent[0], coordTransformed);

	//TODO - culling should be done here.
	//TODO - viewport transform?

	int continuation = 0;
	if(vtxFormat==2 && !tempVertInfo.first)
		continuation = 2;
	else if(vtxFormat==3 && !tempVertInfo.first)
		continuation = 2;


	//record the vertex
	//VERT &vert = tempVertList.list[tempVertList.count];
	VERT &vert = vertlist->list[vertlist->count + tempVertInfo.count - continuation];

	vert.texcoord[0] = last_s;
	vert.texcoord[1] = last_t;
	vert.coord[0] = coordTransformed[0];
	vert.coord[1] = coordTransformed[1];
	vert.coord[2] = coordTransformed[2];
	vert.coord[3] = coordTransformed[3];
	vert.color[0] = colorRGB[0];
	vert.color[1] = colorRGB[1];
	vert.color[2] = colorRGB[2];
	tempVertInfo.map[tempVertInfo.count] = vertlist->count + tempVertInfo.count - continuation;
	tempVertInfo.count++;

	//possibly complete a polygon
	{
		int completed=0;
		switch(vtxFormat) {
			case 0: //GL_TRIANGLES
				if(tempVertInfo.count!=3)
					break;
				completed = 1;
				//vertlist->list[polylist->list[polylist->count].vertIndexes[i] = vertlist->count++] = tempVertList.list[n];
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,2);
				vertlist->count+=3;
				polylist->list[polylist->count].type = 3;
				tempVertInfo.count = 0;
				break;
			case 1: //GL_QUADS
				if(tempVertInfo.count!=4)
					break;
				completed = 1;
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,2);
				SUBMITVERTEX(3,3);
				vertlist->count+=4;
				polylist->list[polylist->count].type = 4;
				tempVertInfo.count = 0;
				break;
			case 2: //GL_TRIANGLE_STRIP
				if(tempVertInfo.count!=3)
					break;
				completed = 1;
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,2);
				polylist->list[polylist->count].type = 3;

				if(triStripToggle)
					tempVertInfo.map[1] = vertlist->count+2-continuation;
				else
					tempVertInfo.map[0] = vertlist->count+2-continuation;
				
				if(tempVertInfo.first)
					vertlist->count+=3;
				else
					vertlist->count+=1;

				triStripToggle ^= 1;
				tempVertInfo.first = false;
				tempVertInfo.count = 2;
				break;
			case 3: //GL_QUAD_STRIP
				if(tempVertInfo.count!=4)
					break;
				completed = 1;
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,3);
				SUBMITVERTEX(3,2);
				polylist->list[polylist->count].type = 4;
				tempVertInfo.map[0] = vertlist->count+2-continuation;
				tempVertInfo.map[1] = vertlist->count+3-continuation;
				if(tempVertInfo.first)
					vertlist->count+=4;
				else vertlist->count+=2;
				tempVertInfo.first = false;
				tempVertInfo.count = 2;
				break;
		}

		if(completed)
		{
			POLY &poly = polylist->list[polylist->count];

			poly.polyAttr = polyAttr;
			poly.texParam = textureFormat;
			poly.texPalette = texturePalette;
			poly.viewport = viewport;
			polylist->count++;
		}
	}
}

BOOL gfx3d_glVertex16b(unsigned int v)
{
	if(coordind==0)
	{
		coord[0]		= float16table[v&0xFFFF];
		coord[1]		= float16table[v>>16];

		++coordind;
		return FALSE;
	}

	coord[2]	  = float16table[v&0xFFFF];

	coordind = 0;
	SetVertex ();
	return TRUE;
}

void gfx3d_glVertex10b(u32 v)
{
	coord[0]		= float10Table[v&1023];
	coord[1]		= float10Table[(v>>10)&1023];
	coord[2]		= float10Table[(v>>20)&1023];

	SetVertex ();
}

void gfx3d_glVertex3_cord(unsigned int one, unsigned int two, unsigned int v)
{
	coord[one]		= float16table[v&0xffff];
	coord[two]		= float16table[v>>16];

	SetVertex ();
}

void gfx3d_glVertex_rel(u32 v)
{
	coord[0]		+= float10RelTable[v&1023];
	coord[1]		+= float10RelTable[(v>>10)&1023];
	coord[2]		+= float10RelTable[(v>>20)&1023];

	SetVertex ();
}

// Ignored for now
void gfx3d_glSwapScreen(unsigned int screen)
{
}


int gfx3d_GetNumPolys()
{
	//so is this in the currently-displayed or currently-built list?
	return 0;
}

int gfx3d_GetNumVertex()
{
	//so is this in the currently-displayed or currently-built list?
	return 0;
}



void gfx3d_glPolygonAttrib (u32 val)
{
	if(inBegin) {
		PROGINFO("Set polyattr in the middle of a begin/end pair.\n  (This won't be activated until the next begin)\n");
		//TODO - we need some some similar checking for teximageparam etc.
	}
	polyAttrPending = val;
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
void gfx3d_glMaterial0(u32 val)
{
	dsDiffuse = val&0xFFFF;
	dsAmbient = val>>16;

	if (BIT15(val))
	{
		colorRGB[0] = (val)&0x1F;
		colorRGB[1] = (val>>5)&0x1F;
		colorRGB[2] = (val>>10)&0x1F;
	}
}

void gfx3d_glMaterial1(u32 val)
{
	dsSpecular = val&0xFFFF;
	dsEmission = val>>16;
}

BOOL gfx3d_glShininess (u32 val)
{
	shininessTable[shininessInd++] = ((val & 0xFF) / 256.0f);
	shininessTable[shininessInd++] = (((val >> 8) & 0xFF) / 256.0f);
	shininessTable[shininessInd++] = (((val >> 16) & 0xFF) / 256.0f);
	shininessTable[shininessInd++] = (((val >> 24) & 0xFF) / 256.0f);

	if (shininessInd < 128) return FALSE;
	shininessInd = 0;
	return TRUE;
}

void gfx3d_UpdateToonTable(u8 offset, u16 val)
{
	gfx3d.u16ToonTable[offset] =  val;
}

void gfx3d_UpdateToonTable(u8 offset, u32 val)
{
	gfx3d.u16ToonTable[offset] = val & 0xFFFF;
	gfx3d.u16ToonTable[offset+1] = val >> 8;
}

static void gfx3d_glTexImage_cache()
{
	texCoordinateTransform = (textureFormat>>30);
}
void gfx3d_glTexImage(u32 val)
{
	textureFormat = val;
	gfx3d_glTexImage_cache();
}

void gfx3d_glTexPalette(u32 val)
{
	texturePalette = val;
}

void gfx3d_glTexCoord(u32 val)
{
	_t = (s16)(val>>16);
	_s = (s16)(val&0xFFFF);

	_s /= 16.0f;
	_t /= 16.0f;

	if (texCoordinateTransform == 1)
	{
		last_s =_s*mtxCurrent[3][0] + _t*mtxCurrent[3][4] +
				0.0625f*mtxCurrent[3][8] + 0.0625f*mtxCurrent[3][12];
		last_t =_s*mtxCurrent[3][1] + _t*mtxCurrent[3][5] +
				0.0625f*mtxCurrent[3][9] + 0.0625f*mtxCurrent[3][13];
	}
	else
	{
		last_s=_s;
		last_t=_t;
	}
}

#define vec3dot(a, b)		(((a[0]) * (b[0])) + ((a[1]) * (b[1])) + ((a[2]) * (b[2])))

void gfx3d_glNormal(u32 v)
{
	int i,c;
	ALIGN(16) float normal[4] = { normalTable[v&1023],
						normalTable[(v>>10)&1023],
						normalTable[(v>>20)&1023],
						1};

	if (texCoordinateTransform == 2)
	{
		last_s =(	(normal[0] *mtxCurrent[3][0] + normal[1] *mtxCurrent[3][4] +
					 normal[2] *mtxCurrent[3][8]) + (_s*16.0f)) / 16.0f;
		last_t =(	(normal[0] *mtxCurrent[3][1] + normal[1] *mtxCurrent[3][5] +
					 normal[2] *mtxCurrent[3][9]) + (_t*16.0f)) / 16.0f;
	}

	//use the current normal transform matrix
	MatrixMultVec3x3 (mtxCurrent[2], normal);

	//apply lighting model
	{
		u8 diffuse[3] = {
			(dsDiffuse)&0x1F,
			(dsDiffuse>>5)&0x1F,
			(dsDiffuse>>10)&0x1F };

		u8 ambient[3] = {
			(dsAmbient)&0x1F,
			(dsAmbient>>5)&0x1F,
			(dsAmbient>>10)&0x1F };

		u8 emission[3] = {
			(dsEmission)&0x1F,
			(dsEmission>>5)&0x1F,
			(dsEmission>>10)&0x1F };

		u8 specular[3] = {
			(dsSpecular)&0x1F,
			(dsSpecular>>5)&0x1F,
			(dsSpecular>>10)&0x1F };

		int vertexColor[3] = { emission[0], emission[1], emission[2] };

		for(i=0;i<4;i++) {
			if(!((lightMask>>i)&1))
				continue;

			{
				u8 _lightColor[3] = {
					(lightColor[i])&0x1F,
					(lightColor[i]>>5)&0x1F,
					(lightColor[i]>>10)&0x1F };

				/* This formula is the one used by the DS */
				/* Reference : http://nocash.emubase.de/gbatek.htm#ds3dpolygonlightparameters */

				float diffuseLevel = std::max(0.0f, -vec3dot(cacheLightDirection[i], normal));
				float shininessLevel = pow(std::max(0.0f, vec3dot(-cacheHalfVector[i], normal)), 2);

				if(dsSpecular & 0x8000)
				{
					int shininessIndex = (int)(shininessLevel * 128);
					if(shininessIndex >= (int)ARRAY_SIZE(shininessTable)) {
						//we can't print this right now, because when a game triggers this it triggers it _A_LOT_
						//so wait until we have per-frame diagnostics.
						//this was tested using Princess Debut (US) after proceeding through the intro and getting the tiara.
						//After much research, I determined that this was caused by the game feeding in a totally jacked matrix
						//to mult4x4 from 0x02129B80 (after feeding two other valid matrices)
						//the game seems to internally index these as: ?, 0x37, 0x2B <-- error
						//but, man... this is seriously messed up. there must be something going wrong.
						//maybe it has something to do with what looks like a mirror room effect that is going on during this time?
						//PROGINFO("ERROR: shininess table out of bounds.\n  maybe an emulator error; maybe a non-unit normal; setting to 0\n");
						shininessIndex = 0;
					}
					shininessLevel = shininessTable[shininessIndex];
				}

				for(c = 0; c < 3; c++)
				{
                                  vertexColor[c] += (int)(((specular[c] * _lightColor[c] * shininessLevel)
						+ (diffuse[c] * _lightColor[c] * diffuseLevel)
						+ (ambient[c] * _lightColor[c])) / 31.0f);
				}
			}
		}

		for(c=0;c<3;c++)
			colorRGB[c] = std::min(31,vertexColor[c]);
	}
}


s32 gfx3d_GetClipMatrix (unsigned int index)
{
	float val = MatrixGetMultipliedIndex (index, mtxCurrent[0], mtxCurrent[1]);

	val *= (1<<12);

	return (s32)val;
}

s32 gfx3d_GetDirectionalMatrix (unsigned int index)
{
	int _index = (((index / 3) * 4) + (index % 3));

	return (s32)(mtxCurrent[2][_index]*(1<<12));
}

static void gfx3d_glLightDirection_cache(int index)
{
	u32 v = lightDirection[index];

	// Convert format into floating point value
	cacheLightDirection[index][0] = normalTable[v&1023];
	cacheLightDirection[index][1] = normalTable[(v>>10)&1023];
	cacheLightDirection[index][2] = normalTable[(v>>20)&1023];
	cacheLightDirection[index][3] = 0;

	/* Multiply the vector by the directional matrix */
	MatrixMultVec3x3(mtxCurrent[2], cacheLightDirection[index]);

	/* Calculate the half vector */
	float lineOfSight[4] = {0.0f, 0.0f, -1.0f, 0.0f};
	for(int i = 0; i < 4; i++)
	{
		cacheHalfVector[index][i] = ((cacheLightDirection[index][i] + lineOfSight[i]) / 2.0f);
	}
}

/*
	0-9   Directional Vector's X component (1bit sign + 9bit fractional part)
	10-19 Directional Vector's Y component (1bit sign + 9bit fractional part)
	20-29 Directional Vector's Z component (1bit sign + 9bit fractional part)
	30-31 Light Number                     (0..3)
*/
void gfx3d_glLightDirection (u32 v)
{
	int index = v>>30;

	lightDirection[index] = v;
	gfx3d_glLightDirection_cache(index);
}

void gfx3d_glLightColor (u32 v)
{
	int index = v>>30;
	lightColor[index] = v;
}

void gfx3d_glAlphaFunc(u32 v)
{
	gfx3d.alphaTestRef = v&31;
}

BOOL gfx3d_glBoxTest(u32 v)
{
	u32 gxstat = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600);
	gxstat &= 0xFFFFFFFD;		// clear boxtest bit
	gxstat |= 0x00000001;		// busy
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600, gxstat);

	BTind++;
	if (BTind < 3) 
	{
	/*	if(BTind == 1)
		{
			BTcoords[0] = float16table[v & 0xFFFF];
			BTcoords[1] = float16table[v >> 16];
		}
		else if(BTind == 2)
		{
			BTcoords[2] = float16table[v & 0xFFFF];
			BTcoords[3] = float16table[v >> 16];
		}*/

		return FALSE;
	}
	BTind = 0;

/*	BTcoords[4] = float16table[v & 0xFFFF];
	BTcoords[5] = float16table[v >> 16];

	ALIGN(16) float boxCoords[6][4][4] = {
		// near
		{{BTcoords[0], BTcoords[1], BTcoords[2], 1.0f}, 
			{BTcoords[0]+BTcoords[3], BTcoords[1], BTcoords[2], 1.0f}, 
			{BTcoords[0]+BTcoords[3], BTcoords[1]+BTcoords[4], BTcoords[2], 1.0f}, 
			{BTcoords[0], BTcoords[1]+BTcoords[4], BTcoords[2], 1.0f}},
		// far
		{{BTcoords[0], BTcoords[1], BTcoords[2]+BTcoords[5], 1.0f}, 
			{BTcoords[0]+BTcoords[3], BTcoords[1], BTcoords[2]+BTcoords[5], 1.0f}, 
			{BTcoords[0]+BTcoords[3], BTcoords[1]+BTcoords[4], BTcoords[2]+BTcoords[5], 1.0f}, 
			{BTcoords[0], BTcoords[1]+BTcoords[4], BTcoords[2]+BTcoords[5], 1.0f}},
		// left
		{{BTcoords[0], BTcoords[1], BTcoords[2]+BTcoords[5], 1.0f}, 
			{BTcoords[0], BTcoords[1], BTcoords[2], 1.0f}, 
			{BTcoords[0], BTcoords[1]+BTcoords[4], BTcoords[2], 1.0f}, 
			{BTcoords[0], BTcoords[1]+BTcoords[4], BTcoords[2]+BTcoords[5], 1.0f}},
		// right
		{{BTcoords[0]+BTcoords[3], BTcoords[1], BTcoords[2], 1.0f}, 
			{BTcoords[0]+BTcoords[3], BTcoords[1], BTcoords[2]+BTcoords[5], 1.0f}, 
			{BTcoords[0]+BTcoords[3], BTcoords[1]+BTcoords[4], BTcoords[2]+BTcoords[5], 1.0f}, 
			{BTcoords[0]+BTcoords[3], BTcoords[1]+BTcoords[4], BTcoords[2], 1.0f}},
		// top
		{{BTcoords[0], BTcoords[1], BTcoords[2]+BTcoords[5], 1.0f}, 
			{BTcoords[0]+BTcoords[3], BTcoords[1], BTcoords[2]+BTcoords[5], 1.0f}, 
			{BTcoords[0]+BTcoords[3], BTcoords[1], BTcoords[2], 1.0f}, 
			{BTcoords[0], BTcoords[1], BTcoords[2], 1.0f}},
		// bottom
		{{BTcoords[0], BTcoords[1]+BTcoords[4], BTcoords[2]+BTcoords[5], 1.0f}, 
			{BTcoords[0]+BTcoords[3], BTcoords[1]+BTcoords[4], BTcoords[2]+BTcoords[5], 1.0f}, 
			{BTcoords[0]+BTcoords[3], BTcoords[1]+BTcoords[4], BTcoords[2], 1.0f}, 
			{BTcoords[0], BTcoords[1]+BTcoords[4], BTcoords[2], 1.0f}}
	};

	for(int face = 0; face < 6; face++)
	{
		for(int vtx = 0; vtx < 4; vtx++)
		{
			MatrixMultVec4x4(mtxCurrent[1], boxCoords[face][vtx]);
			MatrixMultVec4x4(mtxCurrent[0], boxCoords[face][vtx]);

			boxCoords[face][vtx][0] = ((boxCoords[face][vtx][0] + boxCoords[face][vtx][3]) / (2 * boxCoords[face][vtx][3]));
			boxCoords[face][vtx][1] = ((boxCoords[face][vtx][1] + boxCoords[face][vtx][3]) / (2 * boxCoords[face][vtx][3]));
			boxCoords[face][vtx][2] = ((boxCoords[face][vtx][2] + boxCoords[face][vtx][3]) / (2 * boxCoords[face][vtx][3]));

		//	if(face==0)INFO("box test: testing face %i, vtx %i: %f %f %f %f\n", face, vtx, 
		//		boxCoords[face][vtx][0], boxCoords[face][vtx][1], boxCoords[face][vtx][2], boxCoords[face][vtx][3]);

			if ((boxCoords[face][vtx][0] >= 0.0f) && (boxCoords[face][vtx][0] <= 1.0f) &&
				(boxCoords[face][vtx][1] >= 0.0f) && (boxCoords[face][vtx][1] <= 1.0f) &&
				(boxCoords[face][vtx][2] >= 0.0f) && (boxCoords[face][vtx][2] <= 1.0f))
			{
			//	goto faceInside;
			}
			else
				goto noFaceInside;
		}
	}

	goto faceInside;

faceInside:
	gxstat |= 0x2;

noFaceInside:*/
	gxstat &= 0xFFFFFFFE;		// clear busy bit
	gxstat |= 0x00000002;		// hack
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600, gxstat);
	return TRUE;
}

BOOL gfx3d_glPosTest(u32 v)
{
	u32 gxstat = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600);
	gxstat |= 0x00000001;		// busy
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600, gxstat);

	PTind++;
	if (PTind < 2)
	{
		PTcoords[0] = float16table[v & 0xFFFF];
		PTcoords[1] = float16table[v >> 16];

		return FALSE;
	}
	PTind = 0;
	
	PTcoords[2] = float16table[v & 0xFFFF];
	PTcoords[3] = 1.0f;

	MatrixMultVec4x4(mtxCurrent[1], PTcoords);
	MatrixMultVec4x4(mtxCurrent[0], PTcoords);

	gxstat &= 0xFFFFFFFE;
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600, gxstat);

	return TRUE;
}

void gfx3d_glVecTest(u32 v)
{
	u32 gxstat = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600);
	gxstat &= 0xFFFFFFFE;
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600, gxstat);

	//INFO("NDS_glVecTest\n");
}

unsigned int gfx3d_glGetPosRes(unsigned int index)
{
	return (unsigned int)(PTcoords[index] * 4096.0f);
}

unsigned short gfx3d_glGetVecRes(unsigned int index)
{
	//INFO("NDS_glGetVecRes\n");
	return 0;
}

#ifdef USE_GEOMETRY_FIFO_EMULATION

void gfx3d_execute(u8 cmd, u32 param)
{
	switch (cmd)
	{
		case 0x10:		// MTX_MODE - Set Matrix Mode (W)
			gfx3d_glMatrixMode(param);
		break;
		case 0x11:		// MTX_PUSH - Push Current Matrix on Stack (W)
			gfx3d_glPushMatrix();
		break;
		case 0x12:		// MTX_POP - Pop Current Matrix from Stack (W)
			gfx3d_glPopMatrix(param);
		break;
		case 0x13:		// MTX_STORE - Store Current Matrix on Stack (W)
			gfx3d_glStoreMatrix(param);
		break;
		case 0x14:		// MTX_RESTORE - Restore Current Matrix from Stack (W)
			gfx3d_glRestoreMatrix(param);
		break;
		case 0x15:		// MTX_IDENTITY - Load Unit Matrix to Current Matrix (W)
			gfx3d_glLoadIdentity();
		break;
		case 0x16:		// MTX_LOAD_4x4 - Load 4x4 Matrix to Current Matrix (W)
			gfx3d_glLoadMatrix4x4(param);
		break;
		case 0x17:		// MTX_LOAD_4x3 - Load 4x3 Matrix to Current Matrix (W)
			gfx3d_glLoadMatrix4x3(param);
		break;
		case 0x18:		// MTX_MULT_4x4 - Multiply Current Matrix by 4x4 Matrix (W)
			gfx3d_glMultMatrix4x4(param);
		break;
		case 0x19:		// MTX_MULT_4x3 - Multiply Current Matrix by 4x3 Matrix (W)
			gfx3d_glMultMatrix4x3(param);
		break;
		case 0x1A:		// MTX_MULT_3x3 - Multiply Current Matrix by 3x3 Matrix (W)
			gfx3d_glMultMatrix3x3(param);
		break;
		case 0x1B:		// MTX_SCALE - Multiply Current Matrix by Scale Matrix (W)
			gfx3d_glScale(param);
		break;
		case 0x1C:		// MTX_TRANS - Mult. Curr. Matrix by Translation Matrix (W)
			gfx3d_glTranslate(param);
		break;
		case 0x20:		// COLOR - Directly Set Vertex Color (W)
			gfx3d_glColor3b(param);
		break;
		case 0x21:		// NORMAL - Set Normal Vector (W)
			gfx3d_glNormal(param);
		break;
		case 0x22:		// TEXCOORD - Set Texture Coordinates (W)
			gfx3d_glTexCoord(param);
		break;
		case 0x23:		// VTX_16 - Set Vertex XYZ Coordinates (W)
			gfx3d_glVertex16b(param);
		break;
		case 0x24:		// VTX_10 - Set Vertex XYZ Coordinates (W)
			gfx3d_glVertex10b(param);
		break;
		case 0x25:		// VTX_XY - Set Vertex XY Coordinates (W)
			gfx3d_glVertex3_cord(0, 1, param);
		break;
		case 0x26:		// VTX_XZ - Set Vertex XZ Coordinates (W)
			gfx3d_glVertex3_cord(0, 2, param);
		break;
		case 0x27:		// VTX_YZ - Set Vertex YZ Coordinates (W)
			gfx3d_glVertex3_cord(1, 2, param);
		break;
		case 0x28:		// VTX_DIFF - Set Relative Vertex Coordinates (W)
			gfx3d_glVertex_rel(param);
		break;
		case 0x29:		// POLYGON_ATTR - Set Polygon Attributes (W)
			gfx3d_glPolygonAttrib(param);
		break;
		case 0x2A:		// TEXIMAGE_PARAM - Set Texture Parameters (W)
			gfx3d_glTexImage(param);
		break;
		case 0x2B:		// PLTT_BASE - Set Texture Palette Base Address (W)
			gfx3d_glTexPalette(param);
		break;
		case 0x30:		// DIF_AMB - MaterialColor0 - Diffuse/Ambient Reflect. (W)
			gfx3d_glMaterial0(param);
		break;
		case 0x31:		// SPE_EMI - MaterialColor1 - Specular Ref. & Emission (W)
			gfx3d_glMaterial1(param);
		break;
		case 0x32:		// LIGHT_VECTOR - Set Light's Directional Vector (W)
			gfx3d_glLightDirection(param);
		break;
		case 0x33:		// LIGHT_COLOR - Set Light Color (W)
			gfx3d_glLightColor(param);
		break;
		case 0x34:		// SHININESS - Specular Reflection Shininess Table (W)
			gfx3d_glShininess(param);
		break;
		case 0x40:		// BEGIN_VTXS - Start of Vertex List (W)
			gfx3d_glBegin(param);
		break;
		case 0x41:		// END_VTXS - End of Vertex List (W)
			gfx3d_glEnd();
		break;
		case 0x50:		// SWAP_BUFFERS - Swap Rendering Engine Buffer (W)
			gfx3d_glFlush(param);
		break;
		case 0x60:		// VIEWPORT - Set Viewport (W)
			gfx3d_glViewPort(param);
		break;
		case 0x70:		// BOX_TEST - Test if Cuboid Sits inside View Volume (W)
			gfx3d_glBoxTest(param);
		break;
		case 0x71:		// POS_TEST - Set Position Coordinates for Test (W)
			gfx3d_glPosTest(param);
		break;
		case 0x72:		// VEC_TEST - Set Directional Vector for Test (W)
			gfx3d_glVecTest(param);
		break;
		default:
			INFO("Unknown execute FIFO 3D command 0x%02X with param 0x%08X\n", cmd, param);
		break;
	}
}
#endif

static void gfx3d_FlushFIFO()
{
	if (!gxFIFO.tail) return;

#ifdef USE_GEOMETRY_FIFO_EMULATION
	for (int i=0; i < gxFIFO.tail; i++)
	{
		//INFO("3D execute command 0x%02X with param 0x%08X (pos %03i)\n", gxFIFO.cmd[i], gxFIFO.param[i], i);	
		gfx3d_execute(gxFIFO.cmd[i], gxFIFO.param[i]);
	}
#endif
	GFX_FIFOclear();
}

void gfx3d_glFlush(u32 v)
{
	flushPending = TRUE;
	gfx3d.sortmode = BIT0(v);
	gfx3d.wbuffer = BIT1(v);

	//see discussion at top of file
	if(CommonSettings.gfx3d_flushMode == 0)
		gfx3d_doFlush();
}

static bool gfx3d_ysort_compare(int num1, int num2)
{
	const POLY &poly1 = polylist->list[num1];
	const POLY &poly2 = polylist->list[num2];

	//this may be verified by checking the game create menus in harvest moon island of happiness
	if (poly1.maxy > poly2.maxy) return false;
	if (poly1.maxy < poly2.maxy) return true;
	if (poly1.miny < poly2.miny) return true;
	if (poly1.miny > poly2.miny) return false;
	
	//make sure we respect the game's ordering in cases of complete ties
	//this makes it a stable sort.
	//this must be a stable sort or else advance wars DOR will flicker in the main map mode
	if (num1 < num2) return true;
	else return false;
}

static void gfx3d_doFlush()
{
	gfx3d.frameCtr++;

	gfx3d_FlushFIFO();

	// reset
	clInd = 0;
	clCmd = 0;

	//the renderer wil lget the lists we just built
	gfx3d.polylist = polylist;
	gfx3d.vertlist = vertlist;

	int polycount = polylist->count;

	//find the min and max y values for each poly.
	//TODO - this could be a small waste of time if we are manual sorting the translucent polys
	for(int i=0; i<polycount; i++)
	{
		POLY &poly = polylist->list[i];
		float verty = 0.0f; 
		poly.miny = poly.maxy = vertlist->list[poly.vertIndexes[0]].y;

		for(int j=1; j<poly.type; j++)
		{
			verty = vertlist->list[poly.vertIndexes[j]].y;
			poly.miny = min(poly.miny, verty);
			poly.maxy = max(poly.maxy, verty);
		}
	}

	//we need to sort the poly list with alpha polys last
	//first, look for opaque polys
	int ctr=0;
	for(int i=0;i<polycount;i++) {
		POLY &poly = polylist->list[i];
		if(!poly.isTranslucent())
			gfx3d.indexlist[ctr++] = i;
	}
	int opaqueCount = ctr;
	//then look for translucent polys
	for(int i=0;i<polycount;i++) {
		POLY &poly = polylist->list[i];
		if(poly.isTranslucent())
			gfx3d.indexlist[ctr++] = i;
	}

	//now we have to sort the opaque polys by y-value.
	//(test case: harvest moon island of happiness character cretor UI)
	//should this be done after clipping??
	std::sort(gfx3d.indexlist, gfx3d.indexlist + opaqueCount, gfx3d_ysort_compare);
	
	if(!gfx3d.sortmode)
	{
		//if we are autosorting translucent polys, we need to do this also
		//TODO - this is unverified behavior. need a test case
		std::sort(gfx3d.indexlist + opaqueCount, gfx3d.indexlist + polycount, gfx3d_ysort_compare);
	}

	//switch to the new lists
	twiddleLists();

	flushPending = FALSE;
	drawPending = TRUE;
}

void gfx3d_VBlankSignal()
{
	//the 3d buffers are swapped when a vblank begins.
	//so, if we have a redraw pending, now is a safe time to do it
	if(!flushPending)
	{
		gfx3d_FlushFIFO();
		return;
	}

	//see discussion at top of file
	if(CommonSettings.gfx3d_flushMode == 1)
		gfx3d_doFlush();
}

void gfx3d_VBlankEndSignal(bool skipFrame)
{
	//if we are skipping 3d frames then the 3d rendering will get held up here.
	//but, as soon as we quit skipping frames, the held-up 3d frame will render
	if(skipFrame) return;
	if(!drawPending) return;

	drawPending = FALSE;

	if(CommonSettings.showGpu.main)
		gpu3D->NDS_3D_Render();

	//if the null 3d core is chosen, then we need to clear out the 3d buffers to keep old data from being rendered
	if(gpu3D == &gpu3DNull || !CommonSettings.showGpu.main)
	{
		memset(gfx3d_convertedScreen,0,sizeof(gfx3d_convertedScreen));
		memset(gfx3d_convertedScreen,0,sizeof(gfx3d_convertedAlpha));
	}
}

#ifdef USE_GEOMETRY_FIFO_EMULATION
//#define _3D_LOG
static void NOPARAMS()
{
	for (;;)
	{
		if (!clInd) return;
		switch (clCmd & 0xFF)
		{
			case 0x00:
				{
					clCmd >>= 8;
					clInd--;
#ifdef _3D_LOG
					INFO("GX FIFO !!!ZERO!!! (%03i) (without params)\n", gxFIFO.tail);
#endif
					//gfx3d_FlushFIFO();
					continue;
				}
			case 0x11:		// MTX_PUSH - Push Current Matrix on Stack (W)
			case 0x15:		// MTX_IDENTITY - Load Unit Matrix to Current Matrix (W)
			case 0x41:		// END_VTXS - End of Vertex List (W)
				{
#ifdef _3D_LOG
					INFO("GX FIFO cmd 0x%02X (without params)\n", clCmd, gxFIFO.tail);
#endif
					GFX_FIFOsend(clCmd & 0xFF, 0);
					clCmd >>= 8;
					clInd--;
					continue;
				}
		}
		break;
	}
}

void gfx3d_sendCommandToFIFO(u32 val)
{
	if (!clInd)
	{
		if (val == 0)
		{
			//gfx3d_FlushFIFO();
			return;
		}
#ifdef _3D_LOG
		INFO("GX FIFO cmd 0x%02X, gxstat 0x%08X (%03i)\n", val, gxstat, gxFIFO.tail);
#endif
		clCmd = val;

		if (!(clCmd & 0xFFFFFF00))	// unpacked command
			clInd = 1;
		else
			if (!(clCmd & 0xFFFF0000))	// packed command
				clInd = 2;
			else
				if (!(clCmd & 0xFF000000))	// packed command
					clInd = 3;
				else
					clInd = 4;
		NOPARAMS();
		return;
	}

	switch (clCmd & 0xFF)
	{
		case 0x34:		// SHININESS - Specular Reflection Shininess Table (W)
			GFX_FIFOsend(clCmd & 0xFF, val);

			clInd2++;
			if (clInd2 < 32) return;
			clInd2 = 0;
			clCmd >>= 8;
			clInd--;
		break;

		case 0x16:		// MTX_LOAD_4x4 - Load 4x4 Matrix to Current Matrix (W)
		case 0x18:		// MTX_MULT_4x4 - Multiply Current Matrix by 4x4 Matrix (W)
			GFX_FIFOsend(clCmd & 0xFF, val);

			clInd2++;
			if (clInd2 < 16) return;
			clInd2 = 0;
			clCmd >>= 8;
			clInd--;
		break;

		case 0x17:		// MTX_LOAD_4x3 - Load 4x3 Matrix to Current Matrix (W)
		case 0x19:		// MTX_MULT_4x3 - Multiply Current Matrix by 4x3 Matrix (W)
			GFX_FIFOsend(clCmd & 0xFF, val);

			clInd2++;
			if (clInd2 < 12) return;
			clInd2 = 0;
			clCmd >>= 8;
			clInd--;
		break;

		case 0x1A:		// MTX_MULT_3x3 - Multiply Current Matrix by 3x3 Matrix (W)
			GFX_FIFOsend(clCmd & 0xFF, val);

			clInd2++;
			if (clInd2 < 9) return;
			clInd2 = 0;
			clCmd >>= 8;
			clInd--;
		break;

		case 0x1B:		// MTX_SCALE - Multiply Current Matrix by Scale Matrix (W)
		case 0x1C:		// MTX_TRANS - Mult. Curr. Matrix by Translation Matrix (W)
		case 0x70:		// BOX_TEST - Test if Cuboid Sits inside View Volume (W)
			GFX_FIFOsend(clCmd & 0xFF, val);
			
			clInd2++;
			if (clInd2 < 3) return;
			clInd2 = 0;
			clCmd >>= 8;
			clInd--;
		break;

		case 0x23:		// VTX_16 - Set Vertex XYZ Coordinates (W)
		case 0x71:		// POS_TEST - Set Position Coordinates for Test (W)
			GFX_FIFOsend(clCmd & 0xFF, val);
			
			clInd2++;
			if (clInd2 < 2) return;
			clInd2 = 0;
			clCmd >>= 8;
			clInd--;
		break;

		case 0x10:		// MTX_MODE - Set Matrix Mode (W)
		case 0x12:		// MTX_POP - Pop Current Matrix from Stack (W)
		case 0x13:		// MTX_STORE - Store Current Matrix on Stack (W)
		case 0x14:		// MTX_RESTORE - Restore Current Matrix from Stack (W)
		case 0x20:		// COLOR - Directly Set Vertex Color (W)
		case 0x21:		// NORMAL - Set Normal Vector (W)
		case 0x22:		// TEXCOORD - Set Texture Coordinates (W)
		case 0x24:		// VTX_10 - Set Vertex XYZ Coordinates (W)
		case 0x25:		// VTX_XY - Set Vertex XY Coordinates (W)
		case 0x26:		// VTX_XZ - Set Vertex XZ Coordinates (W)
		case 0x27:		// VTX_YZ - Set Vertex YZ Coordinates (W)
		case 0x28:		// VTX_DIFF - Set Relative Vertex Coordinates (W)
		case 0x29:		// POLYGON_ATTR - Set Polygon Attributes (W)
		case 0x2A:		// TEXIMAGE_PARAM - Set Texture Parameters (W)
		case 0x2B:		// PLTT_BASE - Set Texture Palette Base Address (W)
		case 0x30:		// DIF_AMB - MaterialColor0 - Diffuse/Ambient Reflect. (W)
		case 0x31:		// SPE_EMI - MaterialColor1 - Specular Ref. & Emission (W)
		case 0x32:		// LIGHT_VECTOR - Set Light's Directional Vector (W)
		case 0x33:		// LIGHT_COLOR - Set Light Color (W)
		case 0x40:		// BEGIN_VTXS - Start of Vertex List (W)
		case 0x60:		// VIEWPORT - Set Viewport (W)
		case 0x72:		// VEC_TEST - Set Directional Vector for Test (W)
			GFX_FIFOsend(clCmd & 0xFF, val);

			clCmd >>= 8;
			clInd--;
		break;
		case 0x50:		// SWAP_BUFFERS - Swap Rendering Engine Buffer (W)
			gfx3d_glFlush(val);
		break;
		default:
			INFO("Unknown FIFO 3D command 0x%02X (0x%08X)\n", clCmd&0xFF, clCmd);
			clCmd >>= 8;
			clInd--;
			return;
	}

	NOPARAMS();
}

void gfx3d_sendCommand(u32 cmd, u32 param)
{
	cmd = (cmd & 0x01FF) >> 2;
	
#ifdef _3D_LOG
	INFO("GX (dir) cmd 0x%02X = 0x%08X, gxstat 0x%08X (%02i)\n", cmd, param, gxstat, gxFIFO.tail);
#endif

	switch (cmd)
	{
		case 0x10:		// MTX_MODE - Set Matrix Mode (W)
		case 0x11:		// MTX_PUSH - Push Current Matrix on Stack (W)
		case 0x12:		// MTX_POP - Pop Current Matrix from Stack (W)
		case 0x13:		// MTX_STORE - Store Current Matrix on Stack (W)
		case 0x14:		// MTX_RESTORE - Restore Current Matrix from Stack (W)
		case 0x15:		// MTX_IDENTITY - Load Unit Matrix to Current Matrix (W)
		case 0x16:		// MTX_LOAD_4x4 - Load 4x4 Matrix to Current Matrix (W)
		case 0x17:		// MTX_LOAD_4x3 - Load 4x3 Matrix to Current Matrix (W)
		case 0x18:		// MTX_MULT_4x4 - Multiply Current Matrix by 4x4 Matrix (W)
		case 0x19:		// MTX_MULT_4x3 - Multiply Current Matrix by 4x3 Matrix (W)
		case 0x1A:		// MTX_MULT_3x3 - Multiply Current Matrix by 3x3 Matrix (W)
		case 0x1B:		// MTX_SCALE - Multiply Current Matrix by Scale Matrix (W)
		case 0x1C:		// MTX_TRANS - Mult. Curr. Matrix by Translation Matrix (W)
		case 0x20:		// COLOR - Directly Set Vertex Color (W)
		case 0x21:		// NORMAL - Set Normal Vector (W)
		case 0x22:		// TEXCOORD - Set Texture Coordinates (W)
		case 0x23:		// VTX_16 - Set Vertex XYZ Coordinates (W)
		case 0x24:		// VTX_10 - Set Vertex XYZ Coordinates (W)
		case 0x25:		// VTX_XY - Set Vertex XY Coordinates (W)
		case 0x26:		// VTX_XZ - Set Vertex XZ Coordinates (W)
		case 0x27:		// VTX_YZ - Set Vertex YZ Coordinates (W)
		case 0x28:		// VTX_DIFF - Set Relative Vertex Coordinates (W)
		case 0x29:		// POLYGON_ATTR - Set Polygon Attributes (W)
		case 0x2A:		// TEXIMAGE_PARAM - Set Texture Parameters (W)
		case 0x2B:		// PLTT_BASE - Set Texture Palette Base Address (W)
		case 0x30:		// DIF_AMB - MaterialColor0 - Diffuse/Ambient Reflect. (W)
		case 0x31:		// SPE_EMI - MaterialColor1 - Specular Ref. & Emission (W)
		case 0x32:		// LIGHT_VECTOR - Set Light's Directional Vector (W)
		case 0x33:		// LIGHT_COLOR - Set Light Color (W)
		case 0x34:		// SHININESS - Specular Reflection Shininess Table (W)
		case 0x40:		// BEGIN_VTXS - Start of Vertex List (W)
		case 0x41:		// END_VTXS - End of Vertex List (W)
		case 0x60:		// VIEWPORT - Set Viewport (W)
		case 0x70:		// BOX_TEST - Test if Cuboid Sits inside View Volume (W)
		case 0x71:		// POS_TEST - Set Position Coordinates for Test (W)
		case 0x72:		// VEC_TEST - Set Directional Vector for Test (W)
			GFX_FIFOsend(cmd, param);
			break;
		case 0x50:		// SWAP_BUFFERS - Swap Rendering Engine Buffer (W)
			gfx3d_glFlush(param);
		break;
		default:
			INFO("Unknown 3D command %03X with param 0x%08X (directport)\n", cmd, param);
			break;
	}
}
#else

static void NOPARAMS()
{
	for (;;)
	{
		if (!clInd) return;
		switch (clCmd & 0xFF)
		{
			case 0x00:
				{
					clCmd >>= 8;
					clInd--;
					continue;
				}
			case 0x11:
				{
					*(u32 *)(ARM9Mem.ARM9_REG + 0x444) = 0;
					gfx3d_glPushMatrix();
					GFX_FIFOsend(clCmd & 0xFF, 0);
					clCmd >>= 8;
					clInd--;
					continue;
				}
			case 0x15:
				{
					*(u32 *)(ARM9Mem.ARM9_REG + 0x454) = 0;
					gfx3d_glLoadIdentity();
					GFX_FIFOsend(clCmd & 0xFF, 0);
					clCmd >>= 8;
					clInd--;
					continue;
				}
			case 0x41:
				{
					*(u32 *)(ARM9Mem.ARM9_REG + 0x504) = 0;
					gfx3d_glEnd();
					GFX_FIFOsend(clCmd & 0xFF, 0);
					clCmd >>= 8;
					clInd--;
					continue;
				}
		}
		break;
	}
}

void gfx3d_sendCommandToFIFO(u32 val)
{
	if (!clInd)
	{
		if (val == 0) return;

#ifdef _3D_LOG
		INFO("GFX FIFO: Send GFX 3D cmd 0x%02X to FIFO (0x%08X)\n", clCmd, val);
#endif
		clCmd = val;
		if (!(clCmd & 0xFFFFFF00))	// unpacked command
			clInd = 1;
		else
			if (!(clCmd & 0xFFFF0000))	// packed command
				clInd = 2;
			else
				if (!(clCmd & 0xFF000000))	// packed command
					clInd = 3;
				else
					clInd = 4;
		NOPARAMS();
		return;
	}

	switch (clCmd & 0xFF)
	{
		case 0x10:		// MTX_MODE - Set Matrix Mode (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x440) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glMatrixMode(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x12:		// MTX_POP - Pop Current Matrix from Stack (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x448) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glPopMatrix(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x13:		// MTX_STORE - Store Current Matrix on Stack (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x44C) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glStoreMatrix(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x14:		// MTX_RESTORE - Restore Current Matrix from Stack (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x450) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glRestoreMatrix(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x16:		// MTX_LOAD_4x4 - Load 4x4 Matrix to Current Matrix (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x458) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			if (!gfx3d_glLoadMatrix4x4(val)) break;
			clCmd >>= 8;
			clInd--;
		break;
		case 0x17:		// MTX_LOAD_4x3 - Load 4x3 Matrix to Current Matrix (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x45C) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			if (!gfx3d_glLoadMatrix4x3(val)) break;
			clCmd >>= 8;
			clInd--;
		break;
		case 0x18:		// MTX_MULT_4x4 - Multiply Current Matrix by 4x4 Matrix (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x460) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			if (!gfx3d_glMultMatrix4x4(val)) break;
			clCmd >>= 8;
			clInd--;
		break;
		case 0x19:		// MTX_MULT_4x3 - Multiply Current Matrix by 4x3 Matrix (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x464) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			if (!gfx3d_glMultMatrix4x3(val)) break;
			clCmd >>= 8;
			clInd--;
		break;
		case 0x1A:		// MTX_MULT_3x3 - Multiply Current Matrix by 3x3 Matrix (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x468) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			if (!gfx3d_glMultMatrix3x3(val)) break;
			clCmd >>= 8;
			clInd--;
		break;
		case 0x1B:		// MTX_SCALE - Multiply Current Matrix by Scale Matrix (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x46C) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			if (!gfx3d_glScale(val)) break;
			clCmd >>= 8;
			clInd--;
		break;
		case 0x1C:		// MTX_TRANS - Mult. Curr. Matrix by Translation Matrix (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x470) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			if (!gfx3d_glTranslate(val)) break;
			clCmd >>= 8;
			clInd--;
		break;
		case 0x20:		// COLOR - Directly Set Vertex Color (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x480) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glColor3b(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x21:		// NORMAL - Set Normal Vector (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x484) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glNormal(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x22:		// TEXCOORD - Set Texture Coordinates (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x488) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glTexCoord(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x23:		// VTX_16 - Set Vertex XYZ Coordinates (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x48C) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			if (!gfx3d_glVertex16b(val)) break;
			clCmd >>= 8;
			clInd--;
		break;
		case 0x24:		// VTX_10 - Set Vertex XYZ Coordinates (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x490) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glVertex10b(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x25:		// VTX_XY - Set Vertex XY Coordinates (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x494) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glVertex3_cord(0, 1, val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x26:		// VTX_XZ - Set Vertex XZ Coordinates (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x498) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glVertex3_cord(0, 2, val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x27:		// VTX_YZ - Set Vertex YZ Coordinates (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x49C) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glVertex3_cord(1, 2, val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x28:		// VTX_DIFF - Set Relative Vertex Coordinates (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x4A0) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glVertex_rel(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x29:		// POLYGON_ATTR - Set Polygon Attributes (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x4A4) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glPolygonAttrib(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x2A:		// TEXIMAGE_PARAM - Set Texture Parameters (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x4A8) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glTexImage(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x2B:		// PLTT_BASE - Set Texture Palette Base Address (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x4AC) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glTexPalette(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x30:		// DIF_AMB - MaterialColor0 - Diffuse/Ambient Reflect. (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x4C0) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glMaterial0(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x31:		// SPE_EMI - MaterialColor1 - Specular Ref. & Emission (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x4C4) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glMaterial1(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x32:		// LIGHT_VECTOR - Set Light's Directional Vector (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x4C8) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glLightDirection(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x33:		// LIGHT_COLOR - Set Light Color (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x4CC) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glLightColor(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x34:		// SHININESS - Specular Reflection Shininess Table (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x4D0) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			if (!gfx3d_glShininess(val)) break;
			clCmd >>= 8;
			clInd--;
		break;
		case 0x40:		// BEGIN_VTXS - Start of Vertex List (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x500) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glBegin(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x50:		// SWAP_BUFFERS - Swap Rendering Engine Buffer (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x540) = val;
			gfx3d_glFlush(val);
		break;
		case 0x60:		// VIEWPORT - Set Viewport (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x580) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glViewPort(val);
			clCmd >>= 8;
			clInd--;
		break;
		case 0x70:		// BOX_TEST - Test if Cuboid Sits inside View Volume (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x5C0) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			if (!gfx3d_glBoxTest(val)) break;
			clCmd >>= 8;
			clInd--;
		break;
		case 0x71:		// POS_TEST - Set Position Coordinates for Test (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x5C4) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			if (!gfx3d_glPosTest(val)) break;
			clCmd >>= 8;
			clInd--;
		break;
		case 0x72:		// VEC_TEST - Set Directional Vector for Test (W)
			*(u32 *)(ARM9Mem.ARM9_REG + 0x5C8) = val;
			GFX_FIFOsend(clCmd & 0xFF, val);
			gfx3d_glVecTest(val);
			clCmd >>= 8;
			clInd--;
		break;
		default:
			LOG("Unknown FIFO 3D command 0x%02X in cmd=0x%02X\n", clCmd&0xFF, val);
			clCmd >>= 8;
			clInd--;
			break;
	}
	NOPARAMS();
}

void gfx3d_sendCommand(u32 cmd, u32 param)
{
	cmd &= 0x0FFF;
#ifdef _3D_LOG
	INFO("GFX FIFO: Send GFX 3D cmd 0x%02X to FIFO (0x%08X) - DIRECT\n", (cmd & 0x1FF)>>2, param);
#endif

	switch (cmd)
	{
		case 0x340:			// Alpha test reference value - Parameters:1
			gfx3d_glAlphaFunc(param);
		break;
		case 0x350:			// Clear background color setup - Parameters:2
			gfx3d_glClearColor(param);
		break;
		case 0x354:			// Clear background depth setup - Parameters:2
			gfx3d_glClearDepth(param);
		break;
		case 0x356:			// Rear-plane Bitmap Scroll Offsets (W)
		break;
		case 0x358:			// Fog Color - Parameters:4b
			gfx3d_glFogColor(param);
		break;
		case 0x35C:
			gfx3d_glFogOffset(param);
		break;
		case 0x440:		// MTX_MODE - Set Matrix Mode (W)
			gfx3d_glMatrixMode(param);
		break;
		case 0x444:		// MTX_PUSH - Push Current Matrix on Stack (W)
			gfx3d_glPushMatrix();
		break;
		case 0x448:		// MTX_POP - Pop Current Matrix from Stack (W)
			gfx3d_glPopMatrix(param);
		break;
		case 0x44C:		// MTX_STORE - Store Current Matrix on Stack (W)
			gfx3d_glStoreMatrix(param);
		break;
		case 0x450:		// MTX_RESTORE - Restore Current Matrix from Stack (W)
			gfx3d_glRestoreMatrix(param);
		break;
		case 0x454:		// MTX_IDENTITY - Load Unit Matrix to Current Matrix (W)
			gfx3d_glLoadIdentity();
		break;
		case 0x458:		// MTX_LOAD_4x4 - Load 4x4 Matrix to Current Matrix (W)
			gfx3d_glLoadMatrix4x4(param);
		break;
		case 0x45C:		// MTX_LOAD_4x3 - Load 4x3 Matrix to Current Matrix (W)
			gfx3d_glLoadMatrix4x3(param);
		break;
		case 0x460:		// MTX_MULT_4x4 - Multiply Current Matrix by 4x4 Matrix (W)
			gfx3d_glMultMatrix4x4(param);
		break;
		case 0x464:		// MTX_MULT_4x3 - Multiply Current Matrix by 4x3 Matrix (W)
			gfx3d_glMultMatrix4x3(param);
		break;
		case 0x468:		// MTX_MULT_3x3 - Multiply Current Matrix by 3x3 Matrix (W)
			gfx3d_glMultMatrix3x3(param);
		break;
		case 0x46C:		// MTX_SCALE - Multiply Current Matrix by Scale Matrix (W)
			gfx3d_glScale(param);
		break;
		case 0x470:		// MTX_TRANS - Mult. Curr. Matrix by Translation Matrix (W)
			gfx3d_glTranslate(param);
		break;
		case 0x480:		// COLOR - Directly Set Vertex Color (W)
			gfx3d_glColor3b(param);
		break;
		case 0x484:		// NORMAL - Set Normal Vector (W)
			gfx3d_glNormal(param);
		break;
		case 0x488:		// TEXCOORD - Set Texture Coordinates (W)
			gfx3d_glTexCoord(param);
		break;
		case 0x48C:		// VTX_16 - Set Vertex XYZ Coordinates (W)
			gfx3d_glVertex16b(param);
		break;
		case 0x490:		// VTX_10 - Set Vertex XYZ Coordinates (W)
			gfx3d_glVertex10b(param);
		break;
		case 0x494:		// VTX_XY - Set Vertex XY Coordinates (W)
			gfx3d_glVertex3_cord(0, 1, param);
		break;
		case 0x498:		// VTX_XZ - Set Vertex XZ Coordinates (W)
			gfx3d_glVertex3_cord(0, 2, param);
		break;
		case 0x49C:		// VTX_YZ - Set Vertex YZ Coordinates (W)
			gfx3d_glVertex3_cord(1, 2, param);
		break;
		case 0x4A0:		// VTX_DIFF - Set Relative Vertex Coordinates (W)
			gfx3d_glVertex_rel(param);
		break;
		case 0x4A4:		// POLYGON_ATTR - Set Polygon Attributes (W)
			gfx3d_glPolygonAttrib(param);
		break;
		case 0x4A8:		// TEXIMAGE_PARAM - Set Texture Parameters (W)
			gfx3d_glTexImage(param);
		break;
		case 0x4AC:		// PLTT_BASE - Set Texture Palette Base Address (W)
			gfx3d_glTexPalette(param);
		break;
		case 0x4C0:		// DIF_AMB - MaterialColor0 - Diffuse/Ambient Reflect. (W)
			gfx3d_glMaterial0(param);
		break;
		case 0x4C4:		// SPE_EMI - MaterialColor1 - Specular Ref. & Emission (W)
			gfx3d_glMaterial1(param);
		break;
		case 0x4C8:		// LIGHT_VECTOR - Set Light's Directional Vector (W)
			gfx3d_glLightDirection(param);
		break;
		case 0x4CC:		// LIGHT_COLOR - Set Light Color (W)
			gfx3d_glLightColor(param);
		break;
		case 0x4D0:		// SHININESS - Specular Reflection Shininess Table (W)
			gfx3d_glShininess(param);
		break;
		case 0x500:		// BEGIN_VTXS - Start of Vertex List (W)
			gfx3d_glBegin(param);
		break;
		case 0x504:		// END_VTXS - End of Vertex List (W)
			gfx3d_glEnd();
		break;
		case 0x540:		// SWAP_BUFFERS - Swap Rendering Engine Buffer (W)
			gfx3d_glFlush(param);
		break;
		case 0x580:		// VIEWPORT - Set Viewport (W)
			gfx3d_glViewPort(param);
		break;
		case 0x5C0:		// BOX_TEST - Test if Cuboid Sits inside View Volume (W)
			gfx3d_glBoxTest(param);
		break;
		case 0x5C4:		// POS_TEST - Set Position Coordinates for Test (W)
			gfx3d_glPosTest(param);
		break;
		case 0x5C8:		// VEC_TEST - Set Directional Vector for Test (W)
			gfx3d_glVecTest(param);
		break;
		default:
			LOG("Execute direct Port 3D command %03X in param=0x%08X\n", cmd, param);
			break;
	}
}
#endif

static void gfx3d_Control_cache()
{
	u32 v = control;

	if(BIT1(v)) gfx3d.shading = GFX3D::HIGHLIGHT;
	else gfx3d.shading = GFX3D::TOON;

	gfx3d.enableTexturing = BIT0(v);
	gfx3d.enableAlphaTest = BIT2(v);
	gfx3d.enableAlphaBlending = BIT3(v);
	gfx3d.enableAntialiasing = BIT4(v);
	gfx3d.enableEdgeMarking = BIT5(v);
	gfx3d.enableClearImage = BIT14(v);

	//other junk
	if (v&(1<<14))
	{
		LOG("Enabled BITMAP background mode\n");
	}
}

void gfx3d_Control(u32 v)
{
	control = v;
	gfx3d_Control_cache();

}

//--------------
//other misc stuff
void gfx3d_glGetMatrix(unsigned int m_mode, int index, float* dest)
{
	if(index == -1)
	{
		MatrixCopy(dest, mtxCurrent[m_mode]);
		return;
	}

	MatrixCopy(dest, MatrixStackGetPos(&mtxStack[m_mode], index));
}

void gfx3d_glGetLightDirection(unsigned int index, unsigned int* dest)
{
	*dest = lightDirection[index];
}

void gfx3d_glGetLightColor(unsigned int index, unsigned int* dest)
{
	*dest = lightColor[index];
}

void gfx3d_GetLineData(int line, u16** dst, u8** dstAlpha)
{
	*dst = gfx3d_convertedScreen+((line)<<8);
	if(dstAlpha != NULL)
	{
		*dstAlpha = gfx3d_convertedAlpha+((line)<<8);
	}
}


//http://www.opengl.org/documentation/specs/version1.1/glspec1.1/node17.html
//talks about the state required to process verts in quadlists etc. helpful ideas.
//consider building a little state structure that looks exactly like this describes

SFORMAT SF_GFX3D[]={
	{ "GCTL", 4, 1, &control},
	{ "GPAT", 4, 1, &polyAttr},
	{ "GPAP", 4, 1, &polyAttrPending},
	{ "GINB", 4, 1, &inBegin},
	{ "GTFM", 4, 1, &textureFormat},
	{ "GTPA", 4, 1, &texturePalette},
	{ "GMOD", 4, 1, &mode},
	{ "GMTM", 4,16, mtxTemporal},
	{ "GMCU", 4,64, mtxCurrent},
	{ "ML4I", 1, 1, &ML4x4ind},
	{ "ML3I", 1, 1, &ML4x3ind},
	{ "MM4I", 1, 1, &MM4x4ind},
	{ "MM3I", 1, 1, &MM4x3ind},
	{ "MMxI", 1, 1, &MM3x3ind},
	{ "GCOR", 4, 1, coord},
	{ "GCOI", 1, 1, &coordind},
	{ "GVFM", 4, 1, &vtxFormat},
	{ "GTRN", 4, 4, trans},
	{ "GTRI", 1, 1, &transind},
	{ "GSCA", 4, 4, scale},
	{ "GSCI", 1, 1, &scaleind},
	{ "G_T_", 4, 1, &_t},
	{ "G_S_", 4, 1, &_s},
	{ "GL_T", 4, 1, &last_t},
	{ "GL_S", 4, 1, &last_s},
	{ "GLCM", 4, 1, &clCmd},
	{ "GLIN", 4, 1, &clInd},
#ifdef USE_GEOMETRY_FIFO_EMULATION
	{ "GLI2", 4, 1, &clInd2},
#endif
	{ "GLBT", 4, 1, &BTind},
	{ "GLPT", 4, 1, &PTind},
	{ "GLPC", 4, 4, PTcoords},
	{ "GLF9", 4, 1, &gxFIFO.tail},
	{ "GLF9", 1, 261, &gxFIFO.cmd[0]},
	{ "GLF9", 4, 261, &gxFIFO.param[0]},
	{ "GCOL", 1, 4, colorRGB},
	{ "GLCO", 4, 4, lightColor},
	{ "GLDI", 4, 4, lightDirection},
	{ "GMDI", 2, 1, &dsDiffuse},
	{ "GMAM", 2, 1, &dsAmbient},
	{ "GMSP", 2, 1, &dsSpecular},
	{ "GMEM", 2, 1, &dsEmission},
	{ "GFLP", 4, 1, &flushPending},
	{ "GDRP", 4, 1, &drawPending},
	{ "GSET", 4, 1, &gfx3d.enableTexturing},
	{ "GSEA", 4, 1, &gfx3d.enableAlphaTest},
	{ "GSEB", 4, 1, &gfx3d.enableAlphaBlending},
	{ "GSEX", 4, 1, &gfx3d.enableAntialiasing},
	{ "GSEE", 4, 1, &gfx3d.enableEdgeMarking},
	{ "GSSH", 4, 1, &gfx3d.shading},
	{ "GSWB", 4, 1, &gfx3d.wbuffer},
	{ "GSSM", 4, 1, &gfx3d.sortmode},
	{ "GSAR", 1, 1, &gfx3d.alphaTestRef},
	{ "GSVP", 4, 1, &viewport},
	{ "GSCC", 4, 1, &gfx3d.clearColor},
	{ "GSCD", 4, 1, &gfx3d.clearDepth},
	{ "GSFC", 4, 4, gfx3d.fogColor},
	{ "GSFO", 4, 1, &gfx3d.fogOffset},
	{ "GST2", 2, 32, gfx3d.u16ToonTable},
	{ "GSST", 4, 128, shininessTable},
	{ "GSSI", 4, 1, &shininessInd},
	//------------------------
	{ "GTST", 4, 1, &triStripToggle},
	{ "GTVC", 4, 1, &tempVertInfo.count},
	{ "GTVM", 4, 4, tempVertInfo.map},
	{ "GTVF", 4, 1, &tempVertInfo.first},
	{ "G3CS", 2, 256*192, gfx3d_convertedScreen},
	{ "G3CA", 2, 256*192, gfx3d_convertedAlpha}, 
	{ 0 }
};

//-------------savestate
void gfx3d_savestate(std::ostream* os)
{
	//version
	write32le(2,os);

	//dump the render lists
	OSWRITE(vertlist->count);
	for(int i=0;i<vertlist->count;i++)
		vertlist->list[i].save(os);
	OSWRITE(polylist->count);
	for(int i=0;i<polylist->count;i++)
		polylist->list[i].save(os);

	for(int i=0;i<4;i++)
	{
		OSWRITE(mtxStack[i].position);
		for(int j=0;j<mtxStack[i].size*16+16;j++)
			OSWRITE(mtxStack[i].matrix[j]);
	}
}

bool gfx3d_loadstate(std::istream* is, int size)
{
	int version;
	if(read32le(&version,is) != 1) return false;
	if(size==8) version = 0;


	gfx3d_glPolygonAttrib_cache();
	gfx3d_glTexImage_cache();
	gfx3d_Control_cache();
	gfx3d_glLightDirection_cache(0);
	gfx3d_glLightDirection_cache(1);
	gfx3d_glLightDirection_cache(2);
	gfx3d_glLightDirection_cache(3);

	//jiggle the lists. and also wipe them. this is clearly not the best thing to be doing.
	listTwiddle = 0;
	polylist = &polylists[listTwiddle];
	vertlist = &vertlists[listTwiddle];

	if(version>=1)
	{
		OSREAD(vertlist->count);
		for(int i=0;i<vertlist->count;i++)
			vertlist->list[i].load(is);
		OSREAD(polylist->count);
		for(int i=0;i<polylist->count;i++)
			polylist->list[i].load(is);
	}

	if(version>=2)
	{
		for(int i=0;i<4;i++)
		{
			OSREAD(mtxStack[i].position);
			for(int j=0;j<mtxStack[i].size*16+16;j++)
				OSREAD(mtxStack[i].matrix[j]);
		}
	}

	gfx3d.polylist = &polylists[listTwiddle^1];
	gfx3d.vertlist = &vertlists[listTwiddle^1];
	gfx3d.polylist->count=0;
	gfx3d.vertlist->count=0;

	return true;
}
