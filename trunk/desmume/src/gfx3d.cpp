/*
	Copyright (C) 2008 DeSmuME team

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

//This file implements the geometry engine hardware component.
//This handles almost all of the work of 3d rendering, leaving the renderer
// plugin responsible only for drawing primitives.

#include <algorithm>
#include <math.h>
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
#include "readwrite.h"

GFX3D gfx3d;

//tables that are provided to anyone
CACHE_ALIGN u32 color_15bit_to_24bit[32768];
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

CACHE_ALIGN const u8 material_5bit_to_8bit[] = {
	0x00, 0x08, 0x10, 0x18, 0x21, 0x29, 0x31, 0x39,
	0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD,
	0xC6, 0xCE, 0xD6, 0xDE, 0xE7, 0xEF, 0xF7, 0xFF
};

CACHE_ALIGN const u8 material_3bit_to_8bit[] = {
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF
};

//private acceleration tables
static float float16table[65536];
static float float10Table[1024];
static float float10RelTable[1024];
static float normalTable[1024];

#define fix2float(v)    (((float)((s32)(v))) / (float)(1<<12))
#define fix10_2float(v) (((float)((s32)(v))) / (float)(1<<9))

// Matrix stack handling
static CACHE_ALIGN MatrixStack	mtxStack[4];
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

// Data for basic transforms
static CACHE_ALIGN float	trans[4] = {0.0, 0.0, 0.0, 0.0};
static int		transind = 0;
static CACHE_ALIGN float	scale[4] = {0.0, 0.0, 0.0, 0.0};
static int		scaleind = 0;

//various other registers
static int _t=0, _s=0;
static float last_t, last_s;
static u32 clCmd = 0;
static u32 clInd = 0;
static u32 BTind = 0;
static u32 PTind = 0;
static BOOL GFX_busy = FALSE;

//raw ds format poly attributes
static u32 polyAttr=0,textureFormat=0, texturePalette=0;

//the current vertex color, 5bit values
static int colorRGB[4] = { 31,31,31,31 };

u32 control = 0;

//light state:
static u32 lightColor[4] = {0,0,0,0};
static u32 lightDirection[4] = {0,0,0,0};
//material state:
static u16 dsDiffuse, dsAmbient, dsSpecular, dsEmission;
/* Shininess */
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
POLYLIST polylists[2];
POLYLIST* polylist = &polylists[0];
VERTLIST vertlists[2];
VERTLIST* vertlist = &vertlists[0];
PROJLIST projlists[2];
PROJLIST* projlist = &projlists[0];

int listTwiddle = 1;
int triStripToggle;

//list-building state
struct tmpVertInfo {
	//the number of verts registered in this list
	int count;
	//indices to the main vert list
	int map[4];
} tempVertInfo;


static void twiddleLists() {
	listTwiddle++;
	listTwiddle &= 1;
	polylist = &polylists[listTwiddle];
	vertlist = &vertlists[listTwiddle];
	projlist = &projlists[listTwiddle];
	polylist->count = 0;
	vertlist->count = 0;
	projlist->count = 0;
}

static BOOL flushPending = FALSE;
static BOOL drawPending = FALSE;
//------------------------------------------------------------

static void makeTables() {

	//produce the color bits of a 32bpp color from a DS RGB15 using bit logic (internal use only)
	#define RGB15TO24_BITLOGIC(col) ( (material_5bit_to_8bit[((col)>>10)&0x1F]<<16) | (material_5bit_to_8bit[((col)>>5)&0x1F]<<8) | material_5bit_to_8bit[(col)&0x1F] )

	for(int i=0;i<32768;i++)
		color_15bit_to_24bit[i] = RGB15TO24_BITLOGIC((u16)i);

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
	makeTables();
	gfx3d_reset();
}

void gfx3d_reset()
{
	gfx3d = GFX3D();

	flushPending = FALSE;
	listTwiddle = 1;
	twiddleLists();

	MatrixStackSetMaxSize(&mtxStack[0], 1);		// Projection stack
	MatrixStackSetMaxSize(&mtxStack[1], 31);	// Coordinate stack
	MatrixStackSetMaxSize(&mtxStack[2], 31);	// Directional stack
	MatrixStackSetMaxSize(&mtxStack[3], 1);		// Texture stack

	MatrixInit (mtxCurrent[0]);
	MatrixInit (mtxCurrent[1]);
	MatrixInit (mtxCurrent[2]);
	MatrixInit (mtxCurrent[3]);
	MatrixInit (mtxTemporal);

	clCmd = 0;
	clInd = 0;

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
	
	GFX_FIFOclear();
}

void gfx3d_glViewPort(unsigned long v)
{
	//zero: NHerve messed with this in mod2 and mod3, but im still not sure its perfect. need to research this.
	gfx3d.viewport.x = (v&0xFF);
	gfx3d.viewport.y = (v&0xFF);
	gfx3d.viewport.width = (((v>>16)&0xFF)+1)-(v&0xFF);
	gfx3d.viewport.height = ((v>>24)+1)-((v>>8)&0xFF);
}


void gfx3d_glClearColor(unsigned long v)
{
	gfx3d.clearColor[0] = ((float)(v&0x1F))/31.0f;
	gfx3d.clearColor[1] = ((float)((v>>5)&0x1F))/31.0f;
	gfx3d.clearColor[2] = ((float)((v>>10)&0x1F))/31.0f;
	gfx3d.clearColor[3] = ((float)((v>>16)&0x1F))/31.0f;
}

void gfx3d_glFogColor(unsigned long v)
{
	gfx3d.fogColor[0] = ((float)((v    )&0x1F))/31.0f;
	gfx3d.fogColor[1] = ((float)((v>> 5)&0x1F))/31.0f;
	gfx3d.fogColor[2] = ((float)((v>>10)&0x1F))/31.0f;
	gfx3d.fogColor[3] = ((float)((v>>16)&0x1F))/31.0f;
}

void gfx3d_glFogOffset (unsigned long v)
{
	gfx3d.fogOffset = (float)(v&0xffff);
}

void gfx3d_glClearDepth(unsigned long v)
{
	//Thanks to NHerve
	v &= 0x7FFFF;
	u32 depth24b = (v*0x200)+((v+1)/0x8000)*0x01FF;
	gfx3d.clearDepth = depth24b / ((float)(1<<24));
}

void gfx3d_glMatrixMode(unsigned long v)
{
	mode = (v&3);
}


void gfx3d_glLoadIdentity()
{
	MatrixIdentity (mtxCurrent[mode]);

	if (mode == 2)
		MatrixIdentity (mtxCurrent[1]);
}

BOOL gfx3d_glLoadMatrix4x4(signed long v)
{
	mtxCurrent[mode][ML4x4ind] = fix2float(v);

	++ML4x4ind;
	if(ML4x4ind<16) return FALSE;
	ML4x4ind = 0;

	if (mode == 2)
		MatrixCopy (mtxCurrent[1], mtxCurrent[2]);
	
	return TRUE;
}

BOOL gfx3d_glLoadMatrix4x3(signed long v)
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

void gfx3d_glStoreMatrix(unsigned long v)
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

void gfx3d_glRestoreMatrix(unsigned long v)
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

void gfx3d_glPushMatrix()
{
	//this command always works on both pos and vector when either pos or pos-vector are the current mtx mode
	short mymode = (mode==1?2:mode);

	MatrixStackPushMatrix (&mtxStack[mymode], mtxCurrent[mymode]);
	if(mymode==2)
		MatrixStackPushMatrix (&mtxStack[1], mtxCurrent[1]);
}

void gfx3d_glPopMatrix(signed long i)
{
	//this command always works on both pos and vector when either pos or pos-vector are the current mtx mode
	short mymode = (mode==1?2:mode);

	MatrixCopy (mtxCurrent[mode], MatrixStackPopMatrix (&mtxStack[mode], i));

	if (mymode == 2)
		MatrixCopy (mtxCurrent[1], MatrixStackPopMatrix (&mtxStack[1], i));
}

BOOL gfx3d_glTranslate(signed long v)
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

BOOL gfx3d_glScale(signed long v)
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

BOOL gfx3d_glMultMatrix3x3(signed long v)
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

BOOL gfx3d_glMultMatrix4x3(signed long v)
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

BOOL gfx3d_glMultMatrix4x4(signed long v)
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

void gfx3d_glBegin(unsigned long v)
{
	vtxFormat = v&0x03;
	triStripToggle = 0;
	tempVertInfo.count = 0;
}

void gfx3d_glEnd(void)
{
	tempVertInfo.count = 0;
}

void gfx3d_glColor3b(unsigned long v)
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
					coord[2]*mtxCurrent[3][8]) + _s);
		last_t =((coord[0]*mtxCurrent[3][1] +
					coord[1]*mtxCurrent[3][5] +
					coord[2]*mtxCurrent[3][9]) + _t);
	}

	//refuse to do anything if we have too many verts or polys
	if(vertlist->count >= VERTLIST_SIZE)
		return;
	if(polylist->count >= POLYLIST_SIZE)
		return;

	//apply modelview matrix
	MatrixMultVec4x4 (mtxCurrent[1], coordTransformed);

	//todo - we havent got the whole pipeline working yet, so lets save the projection matrix and let opengl do it
	////apply projection matrix
	//MatrixMultVec4x4 (mtxCurrent[0], coordTransformed);

	////perspective division
	//coordTransformed[0] = (coordTransformed[0] + coordTransformed[3]) / 2 / coordTransformed[3];
	//coordTransformed[1] = (coordTransformed[1] + coordTransformed[3]) / 2 / coordTransformed[3];
	//coordTransformed[2] = (coordTransformed[2] + coordTransformed[3]) / 2 / coordTransformed[3];
	//coordTransformed[3] = 1;

	//TODO - culling should be done here.
	//TODO - viewport transform

	//record the vertex
	//VERT &vert = tempVertList.list[tempVertList.count];
	VERT &vert = vertlist->list[vertlist->count + tempVertInfo.count];
	vert.texcoord[0] = last_s;
	vert.texcoord[1] = last_t;
	vert.coord[0] = coordTransformed[0];
	vert.coord[1] = coordTransformed[1];
	vert.coord[2] = coordTransformed[2];
	vert.coord[3] = coordTransformed[3];
	vert.color[0] = colorRGB[0];
	vert.color[1] = colorRGB[1];
	vert.color[2] = colorRGB[2];
	vert.color[3] = colorRGB[3];
	vert.depth = 0x7FFF * coordTransformed[2];
	tempVertInfo.map[tempVertInfo.count] = vertlist->count + tempVertInfo.count;
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
					tempVertInfo.map[1] = vertlist->count+2;
				else
					tempVertInfo.map[0] = vertlist->count+2;
				
				vertlist->count+=3;

				triStripToggle ^= 1;
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
				tempVertInfo.map[0] = vertlist->count+2;
				tempVertInfo.map[1] = vertlist->count+3;
				vertlist->count+=4;
				tempVertInfo.count = 2;
				break;
		}

		if(completed)
		{
			POLY &poly = polylist->list[polylist->count];
			//todo - dont overrun proj list
			
			//see if the last entry in the proj list matches the current matrix, if there is one.
			if(projlist->count != 0 && 
				//here is an example of something that does not work.
				//(for a speed hack, we consider the matrices different if the first element differs)
				//mtxCurrent[0][0] == projlist->projMatrix[projlist->count-1][0]
				
				//here is what we must do: make sure the matrices are identical
				!MatrixCompare(mtxCurrent[0],projlist->projMatrix[projlist->count-1])
				)
			{
				//it matches. use it
				poly.projIndex = projlist->count-1;
			}
			else
			{
				MatrixCopy(projlist->projMatrix[projlist->count],mtxCurrent[0]);
				poly.projIndex = projlist->count;
				projlist->count++;
			}

			poly.polyAttr = polyAttr;
			poly.texParam = textureFormat;
			poly.texPalette = texturePalette;
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

void gfx3d_glVertex10b(unsigned long v)
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

void gfx3d_glVertex_rel(unsigned long v)
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

static void gfx3d_glPolygonAttrib_cache()
{
	// Light enable/disable
	lightMask = (polyAttr&0xF);

	// texture environment
	envMode = (polyAttr&0x30)>>4;

	//// overwrite depth on alpha pass
	//alphaDepthWrite = BIT11(polyAttr);

	//// depth test function
	//depthFuncMode = depthFunc[BIT14(polyAttr)];

	//// back face culling
	cullingMask = (polyAttr>>6)&3;

	// Alpha value, actually not well handled, 0 should be wireframe
	colorRGB[3] = colorAlpha = ((polyAttr>>16)&0x1F);

	//// polyID
	//polyID = (polyAttr>>24)&0x1F;
}

void gfx3d_glPolygonAttrib (unsigned long val)
{
	polyAttr = val;
	gfx3d_glPolygonAttrib_cache();
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
void gfx3d_glMaterial0(unsigned long val)
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

void gfx3d_glMaterial1(unsigned long val)
{
	dsSpecular = val&0xFFFF;
	dsEmission = val>>16;
}

BOOL gfx3d_glShininess (unsigned long val)
{
	shininessTable[shininessInd++] = ((val & 0xFF) / 256.0f);
	shininessTable[shininessInd++] = (((val >> 8) & 0xFF) / 256.0f);
	shininessTable[shininessInd++] = (((val >> 16) & 0xFF) / 256.0f);
	shininessTable[shininessInd++] = (((val >> 24) & 0xFF) / 256.0f);

	if (shininessInd < 128) return FALSE;
	shininessInd = 0;
	return TRUE;
}

void gfx3d_UpdateToonTable(void* toonTable)
{
	u16* u16toonTable = (u16*)toonTable;
	int i;
	for(i=0;i<32;i++)
		gfx3d.rgbToonTable[i] = RGB15TO32(u16toonTable[i],255);
}

static void gfx3d_glTexImage_cache()
{
	texCoordinateTransform = (textureFormat>>30);
}
void gfx3d_glTexImage(unsigned long val)
{
	textureFormat = val;
	gfx3d_glTexImage_cache();
}

void gfx3d_glTexPalette(unsigned long val)
{
	texturePalette = val;
}

void gfx3d_glTexCoord(unsigned long val)
{
	_t = (s16)(val>>16);
	_s = (s16)(val&0xFFFF);

	if (texCoordinateTransform == 1)
	{
		//last_s =_s*mtxCurrent[3][0] + _t*mtxCurrent[3][4] +
		//		0.0625f*mtxCurrent[3][8] + 0.0625f*mtxCurrent[3][12];
		//last_t =_s*mtxCurrent[3][1] + _t*mtxCurrent[3][5] +
		//		0.0625f*mtxCurrent[3][9] + 0.0625f*mtxCurrent[3][13];

		//zero 9/11/08 - I dunno... I think it needs to be like this to make things look right
		last_s =_s*mtxCurrent[3][0] + _t*mtxCurrent[3][4] +
				mtxCurrent[3][8] + mtxCurrent[3][12];
		last_t =_s*mtxCurrent[3][1] + _t*mtxCurrent[3][5] +
				mtxCurrent[3][9] + mtxCurrent[3][13];
	}
	else
	{
		last_s=_s;
		last_t=_t;
	}
}

#define vec3dot(a, b)		(((a[0]) * (b[0])) + ((a[1]) * (b[1])) + ((a[2]) * (b[2])))

void gfx3d_glNormal(unsigned long v)
{
	int i,c;
	ALIGN(16) float normal[3] = { normalTable[v&1023],
						normalTable[(v>>10)&1023],
						normalTable[(v>>20)&1023]};

	if (texCoordinateTransform == 2)
	{
		last_s =(	(normal[0] *mtxCurrent[3][0] + normal[1] *mtxCurrent[3][4] +
					 normal[2] *mtxCurrent[3][8]) + _s);
		last_t =(	(normal[0] *mtxCurrent[3][1] + normal[1] *mtxCurrent[3][5] +
					 normal[2] *mtxCurrent[3][9]) + _t);
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
					shininessLevel = shininessTable[(int)(shininessLevel * 128)];	
				}

				for(c = 0; c < 3; c++)
				{
					vertexColor[c] += (((specular[c] * _lightColor[c] * shininessLevel)
						+ (diffuse[c] * _lightColor[c] * diffuseLevel)
						+ (ambient[c] * _lightColor[c])) / 31.0f);
				}
			}
		}

		for(c=0;c<3;c++)
			colorRGB[c] = std::min(31,vertexColor[c]);
	}
}


signed long gfx3d_GetClipMatrix (unsigned int index)
{
	float val = MatrixGetMultipliedIndex (index, mtxCurrent[0], mtxCurrent[1]);

	val *= (1<<12);

	return (signed long)val;
}

signed long gfx3d_GetDirectionalMatrix (unsigned int index)
{
	int _index = (((index / 3) * 4) + (index % 3));

	return (signed long)(mtxCurrent[2][_index]*(1<<12));
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
void gfx3d_glLightDirection (unsigned long v)
{
	int index = v>>30;

	lightDirection[index] = v;
	gfx3d_glLightDirection_cache(index);
}

void gfx3d_glLightColor (unsigned long v)
{
	int index = v>>30;
	lightColor[index] = v;
}

void gfx3d_glAlphaFunc(unsigned long v)
{
	gfx3d.alphaTestRef = (v&31)/31.f;
}

BOOL gfx3d_glBoxTest(unsigned long v)
{
	BTind++;
	if (BTind < 3) return FALSE;
	BTind = 0;
	//INFO("BoxTest=%i\n",val);
	return TRUE;
}

BOOL gfx3d_glPosTest(unsigned long v)
{
	PTind++;
	if (BTind < 2) return FALSE;
	PTind = 0;
	//INFO("PosTest=%i\n",val);
	return TRUE;
}

void gfx3d_glVecTest(unsigned long v)
{
	//INFO("NDS_glVecTest\n");
}

void gfx3d_glGetPosRes(unsigned int index)
{
	//INFO("NDS_glGetPosRes\n");
	//return 0;
}

void gfx3d_glGetVecRes(unsigned int index)
{
	//INFO("NDS_glGetVecRes\n");
	//return 0;
}

#if 0
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
			INFO("Unknown execute FIFO 3D command 0x%02X with param 0x%02X\n", cmd&0xFF, param);
		break;
	}
}
#endif

static void gfx3d_FlushFIFO()
{
#if 1
	GFX_FIFOclear();
#else
	u32 cmd;
	u32 param;

	//INFO("GX FIFO tail at %i, GXstat 0x%08X\n", gxFIFO.tail, gxstat);
	if (gxFIFO.tail == 0)
	{
		GFX_FIFOclear();
		return;
	}
	for (int i=0; i< gxFIFO.tail; i++)
	{
		cmd = gxFIFO.cmd[i];
		param = gxFIFO.param[i];
		gfx3d_execute(cmd, param);
	}
	//gxstat = ((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x600>>2];
	//INFO("----------- GX FIFO tail at %i, GXstat 0x%08X\n\n", gxFIFO.tail, gxstat);
	GFX_FIFOclear();
#endif
}


void gfx3d_glFlush(unsigned long v)
{
	//INFO("FIFO size=%i\n", MMU.gfx_fifo.size);
	GFX_busy = TRUE;

	gfx3d_FlushFIFO();			// GX FIFO commands execute

	flushPending = TRUE;
	gfx3d.wbuffer = (v&1)!=0;
	gfx3d.sortmode = ((v>>1)&1)!=0;

	// reset
	clInd = 0;
	clCmd = 0;

	//the renderer wil lget the lists we just built
	gfx3d.polylist = polylist;
	gfx3d.vertlist = vertlist;
	gfx3d.projlist = projlist;

	//we need to sort the poly list with alpha polys last
	//first, look for alpha polys
	int polycount = polylist->count;
	int ctr=0;
	for(int i=0;i<polycount;i++) {
		POLY &poly = polylist->list[i];
		if(!poly.isTranslucent())
			gfx3d.indexlist[ctr++] = i;
	}
	//then look for translucent polys
	for(int i=0;i<polycount;i++) {
		POLY &poly = polylist->list[i];
		if(poly.isTranslucent())
			gfx3d.indexlist[ctr++] = i;
	}

	//switch to the new lists
	twiddleLists();

	GFX_busy = FALSE;
}

void gfx3d_VBlankSignal()
{
	//the 3d buffers are swapped when a vblank begins.
	//so, if we have a redraw pending, now is a safe time to do it
	if(!flushPending) return;
	flushPending = FALSE;
	drawPending = TRUE;
}

void gfx3d_VBlankEndSignal()
{
	if(!drawPending) return;
	drawPending = FALSE;
	gpu3D->NDS_3D_Render();
}

#if 0
void gfx3d_sendCommandToFIFO(u32 val)
{
	u32 gxstat = ((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x600>>2];
	//INFO("GX FIFO cmd 0x%08X, gxstat 0x%08X\n", val, gxstat);
	//INFO("FIFO 3D command 0x%02X in 0x%08X (0x%08X)\n", clCmd&0xFF, clCmd, val);
	if (!clInd)
	{
		clCmd = val;
		clInd = 4;
		return;
	}
//	INFO("FIFO 3D command 0x%02X in 0x%08X (0x%08X)\n", clCmd&0xFF, clCmd, val);

	for (;;) 
	{
		switch (clCmd & 0xFF)
		{
			case 0x00:
				{
					if (clInd > 0)
					{
						clCmd >>= 8;
						clInd--;
						gfx3d_FlushFIFO();
						continue;
					}
					gfx3d_FlushFIFO();
					break;
				}
			case 0x11:		// MTX_PUSH - Push Current Matrix on Stack (W)
			case 0x15:		// MTX_IDENTITY - Load Unit Matrix to Current Matrix (W)
			case 0x41:		// END_VTXS - End of Vertex List (W)
				{
					GFX_FIFOsend(clCmd & 0xFF, 0);
					
					clCmd >>= 8;
					clInd--;
					continue;
				}
		}
		break;
	}

	if (!clInd)
	{
		clCmd = val;
		clInd = 4;
		//INFO("GX FIFO cmd 0x%08X, gxstat 0x%08X (%i)\n", clCmd, gxstat, gxFIFO.tail);
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
			
			clCmd >>= 8;
			clInd--;
		break;
		default:
			INFO("Unknown FIFO 3D command 0x%02X in 0x%02X\n", clCmd&0xFF, val);
			clCmd >>= 8;
			clInd--;
			return;
	}

}
#endif

#if 1
static void NOPARAMS(u32 val)
{
	for (;;)
	{
		switch (clCmd & 0xFF)
		{
			case 0x00:
				{
					if (clInd > 0)
					{
						clCmd >>= 8;
						clInd--;
						gfx3d_FlushFIFO();
						continue;
					}
					gfx3d_FlushFIFO();
					clCmd = 0;
					break;
				}
			case 0x11:
				{
					*(u32 *)(ARM9Mem.ARM9_REG + 0x444) = val;
					gfx3d_glPushMatrix();
					GFX_FIFOsend(clCmd & 0xFF, val);
					clCmd >>= 8;
					clInd--;
					continue;
				}
			case 0x15:
				{
					*(u32 *)(ARM9Mem.ARM9_REG + 0x454) = val;
					gfx3d_glLoadIdentity();
					GFX_FIFOsend(clCmd & 0xFF, val);
					clCmd >>= 8;
					clInd--;
					continue;
				}
			case 0x41:
				{
					*(u32 *)(ARM9Mem.ARM9_REG + 0x504) = val;
					gfx3d_glEnd();
					GFX_FIFOsend(clCmd & 0xFF, val);
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
	//INFO("3D command 0x%02X (full val = 0x%08X, val = 0x%08X, ind %i)\n", clCmd&0xFF, clCmd,  val, clInd);
	if (!clInd)
	{
		if (val == 0) 
		{
			gfx3d_FlushFIFO();
			return;
		}
		clCmd = val;
		clInd = 4;
		NOPARAMS(val);
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
			// This is reseted by glFlush, thus not needed here (fixes dsracing.nds)
			//clCmd >>= 8;
			//clInd--;
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
			INFO("Unknown 3D command 0x%02X in cmd=0x%02X\n", clCmd&0xFF, val);
			clCmd >>= 8;
			clInd--;
			break;
	}
	NOPARAMS(val);
}
#endif

void gfx3d_sendCommand(u32 cmd, u32 param)
{
	u32 gxstat = ((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x600>>2];
	if (gxstat & 0x08000000)			// GX busy
						return;
	cmd &= 0x0FFF;

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
			INFO("Execute unknown 3D command %03X in param=0x%08X\n", cmd, param);
			break;
	}
}

#if 0
u32 gfx3d_GetGXstatus()
{
	u32 gxstat = ((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x600>>2];
	//INFO("GFX FIFO read size=%i value 0x%X\n", MMU.gfx_fifo.size, gxstat);
	return (gxstat | 2);
	gxstat &= 0x0F00A000;

	// 0	 BoxTest,PositionTest,VectorTest Busy (0=Ready, 1=Busy)
	if ( (BTind) || (PTind) ) 	gxstat |=  0x01;
	// 1	 BoxTest Result  (0=All Outside View, 1=Parts or Fully Inside View)
	gxstat |= 0x02;
	// 2-7	 Not used
	//
	// 8-12	 Position & Vector Matrix Stack Level (0..31) (lower 5bit of 6bit value)
	gxstat |= 0x00;
	// 13	 Projection Matrix Stack Level        (0..1)
	gxstat |= 0x00;	// w
	// 14	 Matrix Stack Busy (0=No, 1=Yes; Currently executing a Push/Pop command)
	gxstat |= 0x00;
	// 15	 Matrix Stack Overflow/Underflow Error (0=No, 1=Error/Acknowledge/Reset)
	gxstat |= 0x00;	// w
	// 16-24 Number of 40bit-entries in Command FIFO  (0..256)
	//gxstat |= (MMU.gfx_fifo.size & 0xFF) << 16;
	// 24	 Command FIFO Full (MSB of above)  (0=No, 1=Yes; Full)
	gxstat |= MMU.gfx_fifo.full << 24;
	// 25	 Command FIFO Less Than Half Full  (0=No, 1=Yes; Less than Half-full)
	gxstat |= MMU.gfx_fifo.half << 25;
	// 26	 Command FIFO Empty                (0=No, 1=Yes; Empty)
	gxstat |= MMU.gfx_fifo.empty << 26;
	// 27	 Geometry Engine Busy (0=No, 1=Yes; Busy; Commands are executing)
	if (GFX_busy) gxstat |= 0x08000000;
	// 28-29 Not used
	//
	// 30-31 Command FIFO IRQ (0=Never, 1=Less than half full, 2=Empty, 3=Reserved)
	if (MMU.gfx_fifo.empty)
	{
		gxstat |= 2 << 30;
		MMU.reg_IF[ARMCPU_ARM9] |= (1<<21);
	}
	if (MMU.gfx_fifo.half)
	{
		gxstat |= 1 << 30;
		MMU.reg_IF[ARMCPU_ARM9] |= (1<<21);
	}

	//INFO("GFX FIFO read size=%i value 0x%X\n", MMU.gfx_fifo.size, gxstat);
	return gxstat;
}
#endif

static void gfx3d_Control_cache()
{
	u32 v = control;

	if(v&1) gfx3d.enableTexturing = TRUE;
	else gfx3d.enableTexturing = FALSE;

	if((v>>1)&1) gfx3d.shading = GFX3D::HIGHLIGHT;
	else gfx3d.shading = GFX3D::TOON;

	if((v>>2)&1) gfx3d.enableAlphaTest = TRUE;
	else gfx3d.enableAlphaTest = FALSE;

	if((v>>3)&1) gfx3d.enableAlphaBlending = TRUE;
	else gfx3d.enableAlphaBlending = FALSE;

	if((v>>4)&1) gfx3d.enableAntialiasing = TRUE;
	else gfx3d.enableAntialiasing = FALSE;

	if((v>>5)&1) gfx3d.enableEdgeMarking = TRUE;
	else gfx3d.enableEdgeMarking = FALSE;

	//other junk
	if (v&(1<<14))
	{
		LOG("Enabled BITMAP background mode\n");
	}
}

void gfx3d_Control(unsigned long v)
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


//http://www.opengl.org/documentation/specs/version1.1/glspec1.1/node17.html
//talks about the state required to process verts in quadlists etc. helpful ideas.
//consider building a little state structure that looks exactly like this describes

SFORMAT SF_GFX3D[]={
	{ "GCTL", 4, 1, &control},
	{ "GPAT", 4, 1, &polyAttr},
	{ "GTFM", 4, 1, &textureFormat},
	{ "GTPA", 4, 1, &texturePalette},
	{ "GMOD", 4, 1, &mode},
	{ "GMTM", 4,16, mtxTemporal},
	{ "GMCU", 4,64, mtxCurrent},
	{ "GM0P", 4, 1, &mtxStack[0].position},
	{ "GM1M", 4,16, mtxStack[0].matrix},
	{ "GM1P", 4, 1, &mtxStack[1].position},
	{ "GM1M", 4,496,mtxStack[1].matrix},
	{ "GM2P", 4, 1, &mtxStack[2].position},
	{ "GM2M", 4,496,mtxStack[2].matrix},
	{ "GM3P", 4, 1, &mtxStack[3].position},
	{ "GM3M", 4,16, mtxStack[3].matrix},
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
	{ "GLBT", 4, 1, &BTind},
	{ "GLPT", 4, 1, &PTind},
	{ "GLBS", 1, 1, &GFX_busy},
	{ "GLF9", 4, 1, &gxFIFO.tail},
	{ "GLF9", 4, 261, &gxFIFO.cmd},
	{ "GLF9", 4, 261, &gxFIFO.param},
	{ "GCOL", 1, 4, colorRGB},
	{ "GLCO", 4, 4, lightColor},
	{ "GLDI", 4, 4, lightDirection},
	{ "GMDI", 2, 1, &dsDiffuse},
	{ "GMAM", 2, 1, &dsAmbient},
	{ "GMSP", 2, 1, &dsSpecular},
	{ "GMEM", 2, 1, &dsEmission},
	{ "GTST", 4, 1, &triStripToggle},
	{ "GLTW", 4, 1, &listTwiddle},
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
	{ "GSAR", 4, 1, &gfx3d.alphaTestRef},
	{ "GSVX", 4, 1, &gfx3d.viewport.x},
	{ "GSVY", 4, 1, &gfx3d.viewport.y},
	{ "GSVW", 4, 1, &gfx3d.viewport.width},
	{ "GSVH", 4, 1, &gfx3d.viewport.height},
	{ "GSCC", 4, 4, gfx3d.clearColor},
	{ "GSCD", 4, 1, &gfx3d.clearDepth},
	{ "GSFC", 4, 4, gfx3d.fogColor},
	{ "GSFO", 4, 1, &gfx3d.fogOffset},
	{ "GSTT", 4, 32, gfx3d.rgbToonTable},
	{ "GSST", 4, 128, shininessTable},
	{ "GSSI", 4, 1, &shininessInd},
	{ 0 }
};

//-------------savestate
void gfx3d_savestate(std::ostream* os)
{
	//dump the render lists
	//TODO!!!!
}

bool gfx3d_loadstate(std::istream* is)
{
	gfx3d_glPolygonAttrib_cache();
	gfx3d_glTexImage_cache();
	gfx3d_Control_cache();
	gfx3d_glLightDirection_cache(0);
	gfx3d_glLightDirection_cache(1);
	gfx3d_glLightDirection_cache(2);
	gfx3d_glLightDirection_cache(3);

	//jiggle the lists. and also wipe them. this is clearly not the best thing to be doing.
	polylist = &polylists[listTwiddle];
	vertlist = &vertlists[listTwiddle];
	projlist = &projlists[listTwiddle];
	polylist->count = 0;
	vertlist->count = 0;
	projlist->count = 0;
	gfx3d.polylist = &polylists[listTwiddle^1];
	gfx3d.vertlist = &vertlists[listTwiddle^1];
	gfx3d.projlist = &projlists[listTwiddle^1];
	gfx3d.polylist->count=0;
	gfx3d.vertlist->count=0;
	gfx3d.projlist->count = 0;

	return true;
}
