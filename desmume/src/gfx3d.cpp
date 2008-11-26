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
#include "debug.h"
#include "gfx3d.h"
#include "matrix.h"
#include "bits.h"
#include "MMU.h"
#include "render3D.h"
#include "types.h"
#include "saves.h"
#include "readwrite.h"

GFX3D gfx3d;

//tables that are provided to anyone
u32 color_15bit_to_24bit[32768];

//is this a crazy idea? this table spreads 5 bits evenly over 31 from exactly 0 to INT_MAX
const int material_5bit_to_31bit[] = {
	0x00000000, 0x04210842, 0x08421084, 0x0C6318C6,
	0x10842108, 0x14A5294A, 0x18C6318C, 0x1CE739CE,
	0x21084210, 0x25294A52, 0x294A5294, 0x2D6B5AD6,
	0x318C6318, 0x35AD6B5A, 0x39CE739C, 0x3DEF7BDE,
	0x42108421, 0x46318C63, 0x4A5294A5, 0x4E739CE7,
	0x5294A529, 0x56B5AD6B, 0x5AD6B5AD, 0x5EF7BDEF,
	0x6318C631, 0x6739CE73, 0x6B5AD6B5, 0x6F7BDEF7,
	0x739CE739, 0x77BDEF7B, 0x7BDEF7BD, 0x7FFFFFFF
};

const u8 material_5bit_to_8bit[] = {
	0x00, 0x08, 0x10, 0x18, 0x21, 0x29, 0x31, 0x39,
	0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD,
	0xC6, 0xCE, 0xD6, 0xDE, 0xE7, 0xEF, 0xF7, 0xFF
};


const u8 material_3bit_to_8bit[] = {
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
static ALIGN(16) MatrixStack	mtxStack[4];
static ALIGN(16) float		mtxCurrent [4][16];
static ALIGN(16) float		mtxTemporal[16];
static u32 mode = 0;

// Indexes for matrix loading/multiplication
static u8 ML4x4ind = 0;
static u8 ML4x3_c = 0, ML4x3_l = 0;
static u8 MM4x4ind = 0;
static u8 MM4x3_c = 0, MM4x3_l = 0;
static u8 MM3x3_c = 0, MM3x3_l = 0;

// Data for vertex submission
static ALIGN(16) float	coord[4] = {0.0, 0.0, 0.0, 0.0};
static char		coordind = 0;
static u32 vtxFormat;

// Data for basic transforms
static ALIGN(16) float	trans[4] = {0.0, 0.0, 0.0, 0.0};
static u8		transind = 0;
static ALIGN(16) float	scale[4] = {0.0, 0.0, 0.0, 0.0};
static u8		scaleind = 0;

//various other registers
static int _t=0, _s=0;
static float last_t, last_s;
static u32 clCmd = 0;
static u32 clInd = 0;
static u32 clInd2 = 0;

//raw ds format poly attributes
static u32 polyAttr=0,textureFormat=0, texturePalette=0;

//the current vertex color, 5bit values
static u8 colorRGB[4] = { 31,31,31,31 };

u32 control = 0;

//light state:
static u32 lightColor[4] = {0,0,0,0};
static u32 lightDirection[4] = {0,0,0,0};
//material state:
static u16 dsDiffuse, dsAmbient, dsSpecular, dsEmission;


//-----------cached things:
//these dont need to go into the savestate. they can be regenerated from HW registers
//from polygonattr:
static unsigned int cullingMask=0;
static u8 colorAlpha=0;
static u32 envMode=0;
static u32 lightMask=0;
//other things:
static int texCoordinateTransform = 0;
static float cacheLightDirection[4][4];
//------------------

#define RENDER_FRONT_SURFACE 0x80
#define RENDER_BACK_SURFACE 0X40


//-------------poly and vertex lists and such things
POLYLIST polylists[2];
POLYLIST* polylist = &polylists[0];
VERTLIST vertlists[2];
VERTLIST* vertlist = &vertlists[0];

int listTwiddle = 1;
int triStripToggle;

//list-building state
VERTLIST tempVertList;

static void twiddleLists() {
	listTwiddle++;
	listTwiddle &= 1;
	polylist = &polylists[listTwiddle];
	vertlist = &vertlists[listTwiddle];
	polylist->count = 0;
	vertlist->count = 0;
}

static BOOL flushPending = FALSE;
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

void gfx3d_glLoadMatrix4x4(signed long v)
{
	mtxCurrent[mode][ML4x4ind] = fix2float(v);

	++ML4x4ind;
	if(ML4x4ind<16) return;

	if (mode == 2)
		MatrixCopy (mtxCurrent[1], mtxCurrent[2]);

	ML4x4ind = 0;
}

void gfx3d_glLoadMatrix4x3(signed long v)
{
	mtxCurrent[mode][(ML4x3_l<<2)+ML4x3_c] = fix2float(v);

	++ML4x3_c;
	if(ML4x3_c<3) return;

	ML4x3_c = 0;
	++ML4x3_l;
	if(ML4x3_l<4) return;
	ML4x3_l = 0;

	//fill in the unusued matrix values
	mtxCurrent[mode][3] = mtxCurrent[mode][7] = mtxCurrent[mode][11] = 0;
	mtxCurrent[mode][15] = 1;

	if (mode == 2)
		MatrixCopy (mtxCurrent[1], mtxCurrent[2]);
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

void gfx3d_glTranslate(signed long v)
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

void gfx3d_glScale(signed long v)
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

void gfx3d_glMultMatrix3x3(signed long v)
{
	mtxTemporal[(MM3x3_l<<2)+MM3x3_c] = fix2float(v);

	++MM3x3_c;
	if(MM3x3_c<3) return;

	MM3x3_c = 0;
	++MM3x3_l;
	if(MM3x3_l<3) return;
	MM3x3_l = 0;

	//fill in the unusued matrix values
	mtxTemporal[3] = mtxTemporal[7] = mtxTemporal[11] = 0;
	mtxTemporal[15] = 1;
	mtxTemporal[12] = mtxTemporal[13] = mtxTemporal[14] = 0;

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);

	if (mode == 2)
		MatrixMultiply (mtxCurrent[1], mtxTemporal);

	//does this really need to be done?
	MatrixIdentity (mtxTemporal);
}

void gfx3d_glMultMatrix4x3(signed long v)
{
	mtxTemporal[(MM4x3_l<<2)+MM4x3_c] = fix2float(v);

	++MM4x3_c;
	if(MM4x3_c<3) return;

	MM4x3_c = 0;
	++MM4x3_l;
	if(MM4x3_l<4) return;
	MM4x3_l = 0;

	//fill in the unusued matrix values
	mtxTemporal[3] = mtxTemporal[7] = mtxTemporal[11] = 0;
	mtxTemporal[15] = 1;

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);
	if (mode == 2)
		MatrixMultiply (mtxCurrent[1], mtxTemporal);

	//does this really need to be done?
	MatrixIdentity (mtxTemporal);
}

void gfx3d_glMultMatrix4x4(signed long v)
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

void gfx3d_glBegin(unsigned long v)
{
	vtxFormat = v&0x03;
	triStripToggle = 0;
	tempVertList.count = 0;
}

void gfx3d_glEnd(void)
{
	tempVertList.count = 0;
}

void gfx3d_glColor3b(unsigned long v)
{
	colorRGB[0] = (v&0x1F);
	colorRGB[1] = ((v>>5)&0x1F);
	colorRGB[2] = ((v>>10)&0x1F);
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
	VERT &vert = tempVertList.list[tempVertList.count];
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
	tempVertList.count++;

	//possibly complete a polygon
	{
		#define SUBMITVERTEX(i,n) vertlist->list[polylist->list[polylist->count].vertIndexes[i] = vertlist->count++] = tempVertList.list[n];
		int completed=0;
		switch(vtxFormat) {
			case 0: //GL_TRIANGLES
				if(tempVertList.count!=3)
					break;
				completed = 1;
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,2);
				polylist->list[polylist->count].type = 3;
				tempVertList.count = 0;
				break;
			case 1: //GL_QUADS
				if(tempVertList.count!=4)
					break;
				completed = 1;
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,2);
				SUBMITVERTEX(3,3);
				polylist->list[polylist->count].type = 4;
				tempVertList.count = 0;
				break;
			case 2: //GL_TRIANGLE_STRIP
				if(tempVertList.count!=3)
					break;
				completed = 1;
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,2);
				polylist->list[polylist->count].type = 3;
				if(triStripToggle)
					tempVertList.list[1] = tempVertList.list[2];
				else
					tempVertList.list[0] = tempVertList.list[2];
				triStripToggle ^= 1;
				tempVertList.count = 2;
				break;
			case 3: //GL_QUAD_STRIP
				if(tempVertList.count!=4)
					break;
				completed = 1;
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,3);
				SUBMITVERTEX(3,2);
				polylist->list[polylist->count].type = 4;
				tempVertList.list[0] = tempVertList.list[2];
				tempVertList.list[1] = tempVertList.list[3];
				tempVertList.count = 2;
				break;
		}

		if(completed) {
			MatrixCopy(polylist->list[polylist->count].projMatrix,mtxCurrent[0]);
			polylist->list[polylist->count].polyAttr = polyAttr;
			polylist->list[polylist->count].texParam = textureFormat;
			polylist->list[polylist->count].texPalette = texturePalette;
			polylist->count++;
		}
	}
}

void gfx3d_glVertex16b(unsigned int v)
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

void gfx3d_glShininess (unsigned long val)
{
	//INFO("Shininess=%i\n",val);
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

void gfx3d_glNormal(unsigned long v)
{
	int i,c;
	ALIGN(16) float normal[3] = { normalTable[v&1023],
						normalTable[(v>>10)&1023],
						normalTable[(v>>20)&1023]};

	//find the line of sight vector
	//TODO - only do this when the projection matrix changes
	ALIGN(16) float lineOfSight[4] = { 0, 0, -1, 0 };
	MatrixMultVec4x4 (mtxCurrent[0], lineOfSight);

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

		//do we need to normalize lineOfSight?
		Vector3Normalize(lineOfSight);

		for(i=0;i<4;i++) {
			if(!((lightMask>>i)&1))
				continue;

			{
				u8 _lightColor[3] = {
					(lightColor[i])&0x1F,
					(lightColor[i]>>5)&0x1F,
					(lightColor[i]>>10)&0x1F };

				float dot = Vector3Dot(cacheLightDirection[i],normal);
				float diffuseComponent = std::max(0.f,dot);
				float specularComponent;

				//a specular formula which I couldnt get working
				//float halfAngle[3] = {
				//	(lineOfSight[0] + g_lightInfo[i].floatDirection[0])/2,
				//	(lineOfSight[1] + g_lightInfo[i].floatDirection[1])/2,
				//	(lineOfSight[2] + g_lightInfo[i].floatDirection[2])/2};
				//float halfAngleLength = sqrt(halfAngle[0]*halfAngle[0]+halfAngle[1]*halfAngle[1]+halfAngle[2]*halfAngle[2]);
				//float halfAngleNormalized[3] = {
				//	halfAngle[0]/halfAngleLength,
				//	halfAngle[1]/halfAngleLength,
				//	halfAngle[2]/halfAngleLength
				//};
				//
				//float specularAngle = -Vector3Dot(halfAngleNormalized,normal);
				//specularComponent = max(0,cos(specularAngle));

				//a specular formula which seems to work
				float temp[4];
				float diff = Vector3Dot(normal,cacheLightDirection[i]);
				Vector3Copy(temp,normal);
				Vector3Scale(temp,-2*diff);
				Vector3Add(temp,cacheLightDirection[i]);
				Vector3Scale(temp,-1);
				specularComponent = std::max(0.f,Vector3Dot(lineOfSight,temp));

				//if the game isnt producing unit normals, then we can accidentally out of range components. so lets saturate them here
				//so we can at least keep for crashing. we're not sure what the hardware does in this case, but the game shouldnt be doing this.
				specularComponent = std::max(0.f,std::min(1.f,specularComponent));
				diffuseComponent = std::max(0.f,std::min(1.f,diffuseComponent));

				for(c=0;c<3;c++) {
					vertexColor[c] += (diffuseComponent*_lightColor[c]*diffuse[c])/31;
					vertexColor[c] += (specularComponent*_lightColor[c]*specular[c])/31;
					vertexColor[c] += ((float)_lightColor[c]*ambient[c])/31;
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
	index += (index/3);

	return (signed long)(mtxCurrent[2][(index)*(1<<12)]);
}

static void gfx3d_glLightDirection_cache(int index)
{
	u32 v = lightDirection[index];

	// Convert format into floating point value
	cacheLightDirection[index][0] = -normalTable[v&1023];
	cacheLightDirection[index][1] = -normalTable[(v>>10)&1023];
	cacheLightDirection[index][2] = -normalTable[(v>>20)&1023];
	cacheLightDirection[index][3] = 0;
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

void gfx3d_glBoxTest(unsigned long v)
{
}

void gfx3d_glPosTest(unsigned long v)
{
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

void gfx3d_glCallList(unsigned long v)
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
				gfx3d_glPushMatrix();
				--clInd;
				clCmd>>=8;
				continue;
			}

			case 0x15:
			{
				((unsigned long *)ARM9Mem.ARM9_REG)[0x454>>2] = v;
				gfx3d_glLoadIdentity();
				--clInd;
				clCmd>>=8;
				continue;
			}

			case 0x41:
			{
				gfx3d_glEnd();
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
			gfx3d_glMatrixMode (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x12:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x448>>2] = v;
			gfx3d_glPopMatrix(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x13:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x44C>>2] = v;
			gfx3d_glStoreMatrix(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x14:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x450>>2] = v;
			gfx3d_glRestoreMatrix (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x16:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x458>>2] = v;
			gfx3d_glLoadMatrix4x4(v);
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
			gfx3d_glLoadMatrix4x3(v);
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
			gfx3d_glMultMatrix4x4(v);
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
			gfx3d_glMultMatrix4x3(v);
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
			gfx3d_glMultMatrix3x3(v);
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
			gfx3d_glScale (v);
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
			gfx3d_glTranslate (v);
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
			gfx3d_glColor3b(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x21 :
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x484>>2] = v;
			gfx3d_glNormal(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x22 :
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x488>>2] = v;
			gfx3d_glTexCoord(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x23 :
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x48C>>2] = v;
			gfx3d_glVertex16b(v);
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
			gfx3d_glVertex10b(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x25:// GFX_VERTEX_XY
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x494>>2] = v;
			gfx3d_glVertex3_cord(0,1,v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x26:// GFX_VERTEX_XZ
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x498>>2] = v;
			gfx3d_glVertex3_cord(0,2,v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x27:// GFX_VERTEX_YZ
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x49C>>2] = v;
			gfx3d_glVertex3_cord(1,2,v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x28: // GFX_VERTEX_DIFF
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4A0>>2] = v;
			gfx3d_glVertex_rel (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x29:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4A4>>2] = v;
			gfx3d_glPolygonAttrib (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x2A:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4A8>>2] = v;
			gfx3d_glTexImage (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x2B:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4AC>>2] = v;
			gfx3d_glTexPalette (v&0x1FFF);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x30: // GFX_DIFFUSE_AMBIENT
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4C0>>2] = v;
			gfx3d_glMaterial0(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x31: // GFX_SPECULAR_EMISSION
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4C4>>2] = v;
			gfx3d_glMaterial1(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x32:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4C8>>2] = v;
			gfx3d_glLightDirection(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x33:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4CC>>2] = v;
			gfx3d_glLightColor(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x34:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4D0>>2] = v;
			gfx3d_glShininess (v);
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
			gfx3d_glBegin(v);
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
			gfx3d_glViewPort(v);
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
		--clInd;
		clCmd>>=8;
	}
}

void gfx3d_glFlush(unsigned long v)
{
	flushPending = TRUE;
	gfx3d.wbuffer = (v&1)!=0;
	gfx3d.sortmode = ((v>>1)&1)!=0;

	//reset GE state
	clCmd = 0;
	clInd = 0;

	//the renderer wil lget the lists we just built
	gfx3d.polylist = polylist;
	gfx3d.vertlist = vertlist;

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
}

void gfx3d_VBlankSignal()
{
	//the 3d buffers are swapped when a vblank begins.
	//so, if we have a redraw pending, now is a safe time to do it
	if(!flushPending) return;
	flushPending = FALSE;

	gpu3D->NDS_3D_Render();
}

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
	{ "ML3C", 1, 1, &ML4x3_c},
	{ "ML3L", 1, 1, &ML4x3_l},
	{ "MM4I", 1, 1, &MM4x4ind},
	{ "MM3C", 1, 1, &MM4x3_c},
	{ "MM3L", 1, 1, &MM4x3_l},
	{ "MMxC", 1, 1, &MM3x3_c},
	{ "MMxL", 1, 1, &MM3x3_l},
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
	{ "GLI2", 4, 1, &clInd2},
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
	polylist->count = 0;
	vertlist->count = 0;
	gfx3d.polylist = &polylists[listTwiddle^1];
	gfx3d.vertlist = &vertlists[listTwiddle^1];
	gfx3d.polylist->count=0;
	gfx3d.vertlist->count=0;

	return true;
}
