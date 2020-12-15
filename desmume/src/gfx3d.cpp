/*	
	Copyright (C) 2006 yopyop
	Copyright (C) 2008-2019 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

//This file implements the geometry engine hardware component.
//This handles almost all of the work of 3d rendering, leaving the renderer
//plugin responsible only for drawing primitives.

//---------------
//TODO TODO TODO TODO
//make up mind once and for all whether fog, toon, etc. should reside in memory buffers (for easier handling in MMU)
//if they do, then we need to copy them out in doFlush!!!
//---------------

//old tests of matrix stack:
//without the mymode==(texture) namco classics galaxian will try to use pos=1 and overrun the stack, corrupting emu
//according to gbatek, 31 works but sets the stack overflow flag. spider-man 2 tests this on the spiderman model (and elsewhere)
//see "Tony Hawk's American Sk8land" work from revision 07806eb8364d1eb2d21dd3fe5234c9abe0a8a894 
//from MatrixStackSetStackPosition: "once upon a time, we tried clamping to the size. this utterly broke sims 2 apartment pets. changing to wrap around made it work perfectly"
//Water Horse Legend Of The Deep does seem to exercise the texture matrix stack, which does probably need to exist *but I'm not 100% sure)

#include "gfx3d.h"

#include <assert.h>
#include <math.h>
#include <string.h>
#include <algorithm>
#include <queue>

#include "armcpu.h"
#include "debug.h"
#include "driver.h"
#include "emufile.h"
#include "matrix.h"
#include "GPU.h"
#include "MMU.h"
#include "render3D.h"
#include "mem.h"
#include "types.h"
#include "saves.h"
#include "NDSSystem.h"
#include "readwrite.h"
#include "FIFO.h"
#include "utils/bits.h"
#include "movie.h" //only for currframecounter which really ought to be moved into the core emu....

//#define _SHOW_VTX_COUNTERS	// show polygon/vertex counters on screen
#ifdef _SHOW_VTX_COUNTERS
u32 max_polys, max_verts;
#include "GPU_OSD.h"
#endif


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

#define GFX_NOARG_COMMAND 0x00
#define GFX_INVALID_COMMAND 0xFF
#define GFX_UNDEFINED_COMMAND 0xCC
static const u8 gfx3d_commandTypes[] = {
	/* 00 */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, //invalid commands; no parameters
	/* 10 */ 0x01,0x00,0x01,0x01,0x01,0x00,0x10,0x0C, 0x10,0x0C,0x09,0x03,0x03,0xCC,0xCC,0xCC, //matrix commands
	/* 20 */ 0x01,0x01,0x01,0x02,0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01,0xCC,0xCC,0xCC,0xCC, //vertex and per-vertex material commands
	/* 30 */ 0x01,0x01,0x01,0x01,0x20,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, //lighting engine material commands
	/* 40 */ 0x01,0x00,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, //begin and end
	/* 50 */ 0x01,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, //swapbuffers
	/* 60 */ 0x01,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, //viewport
	/* 70 */ 0x03,0x02,0x01,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, //tests
	//0x80:
	/* 80 */ 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
	/* 90 */ 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
	/* A0 */ 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
	/* B0 */ 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
	/* C0 */ 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
	/* D0 */ 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
	/* E0 */ 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
	/* F0 */ 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC
};

class GXF_Hardware
{
public:

	GXF_Hardware()
	{
		reset();
	}

	void reset()
	{
		shiftCommand = 0;
		paramCounter = 0;
	}

	void receive(u32 val) 
	{
		//so, it seems as if the dummy values and restrictions on the highest-order command in the packed command set 
		//is solely about some unknown internal timing quirk, and not about the logical behaviour of the state machine.
		//it's possible that writing some values too quickly can result in the gxfifo not being ready. 
		//this would be especially troublesome when they're getting DMA'd nonstop with no delay.
		//so, since the timing is not emulated rigorously here, and only the logic, we shouldn't depend on or expect the dummy values.
		//indeed, some games aren't issuing them, and will break if they are expected.
		//however, since the dummy values seem to be 0x00000000 always, theyre benign to the state machine, even if the HW timing doesnt require them.
		//the actual logical rule seem to be:
		//1. commands with no arguments are executed as soon as possible; when the packed command is first received, or immediately after the preceding N-arg command.
		//1a. for example, a 0-arg command following an N-arg command doesn't require a dummy value
		//1b. for example, two 0-arg commands constituting a packed command will execute immediately when the packed command is received.
		//
		//as an example, DQ6 entity rendering will issue:
		//0x00151110
		//0x00000000
		//0x00171012 (next packed command)
		//this 0 param is meant for the 0x10 mtxMode command. dummy args aren't supplied for the 0x11 or 0x15.
		//but other times, the game will issue dummy parameters.
		//Either this is because the rules are more complex (do only certain 0-arg commands require dummies?),
		//or the game made a mistake in applying the rules, which didnt seem to irritate the hypothetical HW timing quirk.
		//yet, another time, it will issue the dummy:
		//0x00004123 (XYZ vertex)
		//0x0c000400 (arg1)
		//0x00000000 (arg2)
		//0x00000000 (dummy for 0x41)
		//0x00000017 (next packed command)

		u8 currCommand = shiftCommand & 0xFF;
		u8 currCommandType = gfx3d_commandTypes[currCommand];

		//if the current command is invalid, receive a new packed command.
		if(currCommandType == GFX_INVALID_COMMAND)
		{
			shiftCommand = val;
		}

		//finish receiving args
		if(paramCounter>0)
		{
			GFX_FIFOsend(currCommand, val);
			paramCounter--;
			if(paramCounter <= 0)
				shiftCommand >>= 8;
			else return;
		}

		//analyze current packed commands
		for(;;)
		{
			currCommand = shiftCommand & 0xFF;
			currCommandType = gfx3d_commandTypes[currCommand];

			if(currCommandType == GFX_UNDEFINED_COMMAND)
				shiftCommand >>= 8;
			else if(currCommandType == GFX_NOARG_COMMAND)
			{
				GFX_FIFOsend(currCommand, 0);
				shiftCommand >>= 8;
			}
			else if(currCommand == 0 && shiftCommand!=0)
			{
				//quantum of solace will send a command: 0x001B1100
				//you see NOP in the first command. that needs to get bypassed somehow.
				//since the general goal here is to process packed commands until we have none left (0's will be shifted in)
				//we can just skip this command and continue processing if theres anything left to process.
				shiftCommand >>= 8;
			}
			else if(currCommandType == GFX_INVALID_COMMAND)
			{
				//break when the current command is invalid. 0x00 is invalid; this is what gets used to terminate a loop after all the commands are handled
				break;
			}
			else 
			{
				paramCounter = currCommandType;
				break;
			}
		}
	}

private:

	u32 shiftCommand;
	u32 paramCounter;

public:

	void savestate(EMUFILE &f)
	{
		f.write_32LE(2); //version
		f.write_32LE(shiftCommand);
		f.write_32LE(paramCounter);
	}
	
	bool loadstate(EMUFILE &f)
	{
		u32 version;
		if (f.read_32LE(version) != 1) return false;

		u8 junk8;
		u32 junk32;

		if (version == 0)
		{
			//untested
			f.read_32LE(junk32);
			int commandCursor = 4-junk32;
			for (u32 i=commandCursor;i<4;i++) f.read_u8(junk8);
			f.read_32LE(junk32);
			for (u32 i=commandCursor;i<4;i++) f.read_u8(junk8);
			f.read_u8(junk8);
		}
		else if (version == 1)
		{
			//untested
			f.read_32LE(junk32);
			f.read_32LE(junk32);
			for (u32 i=0;i<4;i++) f.read_u8(junk8);
			for (u32 i=0;i<4;i++) f.read_u8(junk8);
			f.read_u8(junk8);
		}
		else if (version == 2)
		{
			f.read_32LE(shiftCommand);
			f.read_32LE(paramCounter);
		}

		return true;
	}

} gxf_hardware;

//these were 4 for the longest time (this is MUCH, MUCH less than their theoretical values)
//but it was changed to 1 for strawberry shortcake, which was issuing direct commands 
//while the fifo was full, apparently expecting the fifo not to be full by that time.
//in general we are finding that 3d takes less time than we think....
//although maybe the true culprit was charging the cpu less time for the dma.
#define GFX_DELAY(x) NDS_RescheduleGXFIFO(1);
#define GFX_DELAY_M2(x) NDS_RescheduleGXFIFO(1);

using std::max;
using std::min;

GFX3D gfx3d;
Viewer3d_State* viewer3d_state = NULL;
static GFX3D_Clipper boxtestClipper;

//tables that are provided to anyone
CACHE_ALIGN u8 mixTable555[32][32][32];
CACHE_ALIGN u32 dsDepthExtend_15bit_to_24bit[32768];

//private acceleration tables
static float float16table[65536];
static float float10Table[1024];
static float float10RelTable[1024];
static float normalTable[1024];

#define fix2float(v)    (((float)((s32)(v))) / (float)(1<<12))
#define fix10_2float(v) (((float)((s32)(v))) / (float)(1<<9))

// Color buffer that is filled by the 3D renderer and is read by the GPU engine.
static CACHE_ALIGN FragmentColor _gfx3d_savestateBuffer[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];

// Matrix stack handling
//TODO: decouple stack pointers from matrix stack type
CACHE_ALIGN MatrixStack<MATRIXMODE_PROJECTION> mtxStackProjection;
CACHE_ALIGN MatrixStack<MATRIXMODE_POSITION> mtxStackPosition;
CACHE_ALIGN MatrixStack<MATRIXMODE_POSITION_VECTOR> mtxStackPositionVector;
CACHE_ALIGN MatrixStack<MATRIXMODE_TEXTURE> mtxStackTexture;

static CACHE_ALIGN s32 mtxCurrent[4][16];
static CACHE_ALIGN s32 mtxTemporal[16];
static MatrixMode mode = MATRIXMODE_PROJECTION;

// Indexes for matrix loading/multiplication
static u8 ML4x4ind = 0;
static u8 ML4x3ind = 0;
static u8 MM4x4ind = 0;
static u8 MM4x3ind = 0;
static u8 MM3x3ind = 0;

// Data for vertex submission
static CACHE_ALIGN s16		s16coord[4] = {0, 0, 0, 0};
static u8 coordind = 0;
static PolygonPrimitiveType vtxFormat = GFX3D_TRIANGLES;
static BOOL inBegin = FALSE;

// Data for basic transforms
static CACHE_ALIGN s32	trans[4] = {0, 0, 0, 0};
static u8		transind = 0;
static CACHE_ALIGN s32	scale[4] = {0, 0, 0, 0};
static u8		scaleind = 0;
static u32 viewport = 0;

//various other registers
static s32 _t=0, _s=0;
static s32 last_t, last_s;
static u32 clCmd = 0;
static u32 clInd = 0;

static u32 clInd2 = 0;
BOOL isSwapBuffers = FALSE;

static u32 BTind = 0;
static u32 PTind = 0;
static CACHE_ALIGN u16 BTcoords[6] = {0, 0, 0, 0, 0, 0};
static CACHE_ALIGN float PTcoords[4] = {0.0, 0.0, 0.0, 1.0};

//raw ds format poly attributes
static POLYGON_ATTR polyAttrInProcess;
static POLYGON_ATTR currentPolyAttr;
static TEXIMAGE_PARAM currentPolyTexParam;
static u32 currentPolyTexPalette = 0;

//the current vertex color, 5bit values
static u8 colorRGB[4] = { 31,31,31,31 };

//light state:
static u32 lightColor[4] = {0,0,0,0};
static s32 lightDirection[4] = {0,0,0,0};
//material state:
static u16 dsDiffuse, dsAmbient, dsSpecular, dsEmission;
//used for indexing the shininess table during parameters to shininess command
static u8 shininessInd = 0;

// "Freelook" related things
int freelookMode = 0;
s32 freelookMatrix[16];

//-----------cached things:
//these dont need to go into the savestate. they can be regenerated from HW registers
//from polygonattr:
static u32 lightMask=0;
//other things:
static TextureTransformationMode texCoordTransformMode = TextureTransformationMode_None;
static CACHE_ALIGN s32 cacheLightDirection[4][4];
static CACHE_ALIGN s32 cacheHalfVector[4][4];
//------------------

#define RENDER_FRONT_SURFACE 0x80
#define RENDER_BACK_SURFACE 0X40


//-------------poly and vertex lists and such things
POLYLIST* polylists = NULL;
POLYLIST* polylist = NULL;
VERT *vertLists = NULL;
VERT *vertList = NULL;

GFX3D_Clipper *_clipper = NULL;
CPoly _clippedPolyWorkingList[POLYLIST_SIZE * 2];
CPoly _clippedPolyUnsortedList[POLYLIST_SIZE];
CPoly _clippedPolySortedList[POLYLIST_SIZE];

size_t vertListCount[2] = {0, 0};
int polygonListCompleted = 0;

static int listTwiddle = 1;
static u8 triStripToggle;

//list-building state
struct tmpVertInfo
{
	//the number of verts registered in this list
	s32 count;
	//indices to the main vert list
	s32 map[4];
	//indicates that the first poly in a list has been completed
	BOOL first;
} tempVertInfo;


static void twiddleLists()
{
	listTwiddle++;
	listTwiddle &= 1;
	polylist = &polylists[listTwiddle];
	vertList = vertLists + (VERTLIST_SIZE * listTwiddle);
	polylist->count = 0;
	polylist->opaqueCount = 0;
	vertListCount[listTwiddle] = 0;
}

static BOOL drawPending = FALSE;
//------------------------------------------------------------

static void makeTables()
{
	for (size_t i = 0; i < 32768; i++)
	{
		// 15-bit to 24-bit depth formula from http://problemkaputt.de/gbatek.htm#ds3drearplane
		//dsDepthExtend_15bit_to_24bit[i] = LE_TO_LOCAL_32( (i*0x0200) + ((i+1)>>15)*0x01FF );
		
		// Is GBATEK actually correct here? Let's try using a simplified formula and see if it's
		// more accurate.
		dsDepthExtend_15bit_to_24bit[i] = LE_TO_LOCAL_32( (i*0x0200) + 0x01FF );
	}

	for (size_t i = 0; i < 65536; i++)
		float16table[i] = fix2float((signed short)i);

	for (size_t i = 0; i < 1024; i++)
		float10Table[i] = ((signed short)(i<<6)) / (float)(1<<12);

	for (size_t i = 0; i < 1024; i++)
		float10RelTable[i] = ((signed short)(i<<6)) / (float)(1<<18);

	for (size_t i = 0; i < 1024; i++)
		normalTable[i] = ((signed short)(i<<6)) / (float)(1<<15);

	for (size_t r = 0; r <= 31; r++)
		for (size_t oldr = 0; oldr <= 31; oldr++)
			for (size_t a = 0; a <= 31; a++)
			{
				int temp = (r*a + oldr*(31-a)) / 31;
				mixTable555[a][r][oldr] = temp;
			}
}

void POLY::save(EMUFILE &os)
{
	os.write_32LE((u32)type);
	os.write_16LE(vertIndexes[0]);
	os.write_16LE(vertIndexes[1]);
	os.write_16LE(vertIndexes[2]);
	os.write_16LE(vertIndexes[3]);
	os.write_32LE(attribute.value);
	os.write_32LE(texParam.value);
	os.write_32LE(texPalette);
	os.write_32LE(viewport);
	os.write_floatLE(miny);
	os.write_floatLE(maxy);
}

void POLY::load(EMUFILE &is)
{
	u32 polyType32;
	is.read_32LE(polyType32);
	type = (PolygonType)polyType32;
	
	is.read_16LE(vertIndexes[0]);
	is.read_16LE(vertIndexes[1]);
	is.read_16LE(vertIndexes[2]);
	is.read_16LE(vertIndexes[3]);
	is.read_32LE(attribute.value);
	is.read_32LE(texParam.value);
	is.read_32LE(texPalette);
	is.read_32LE(viewport);
	is.read_floatLE(miny);
	is.read_floatLE(maxy);
}

void VERT::save(EMUFILE &os)
{
	os.write_floatLE(x);
	os.write_floatLE(y);
	os.write_floatLE(z);
	os.write_floatLE(w);
	os.write_floatLE(u);
	os.write_floatLE(v);
	os.write_u8(color[0]);
	os.write_u8(color[1]);
	os.write_u8(color[2]);
	os.write_floatLE(fcolor[0]);
	os.write_floatLE(fcolor[1]);
	os.write_floatLE(fcolor[2]);
}
void VERT::load(EMUFILE &is)
{
	is.read_floatLE(x);
	is.read_floatLE(y);
	is.read_floatLE(z);
	is.read_floatLE(w);
	is.read_floatLE(u);
	is.read_floatLE(v);
	is.read_u8(color[0]);
	is.read_u8(color[1]);
	is.read_u8(color[2]);
	is.read_floatLE(fcolor[0]);
	is.read_floatLE(fcolor[1]);
	is.read_floatLE(fcolor[2]);
}

void gfx3d_init()
{
	_clipper = new GFX3D_Clipper;
	_clipper->SetClippedPolyBufferPtr(_clippedPolyWorkingList);
	
	polyAttrInProcess.value = 0;
	currentPolyAttr.value = 0;
	currentPolyTexParam.value = 0;
	
	gxf_hardware.reset();
	//gxf_hardware.test();
	int zzz=9;

	//DWORD start = timeGetTime();
	//for(int i=0;i<1000000000;i++)
	//	MatrixMultVec4x4(mtxCurrent[0],mtxCurrent[1]);
	//DWORD end = timeGetTime();
	//DWORD diff = end-start;

	//start = timeGetTime();
	//for(int i=0;i<1000000000;i++)
	//	MatrixMultVec4x4_b(mtxCurrent[0],mtxCurrent[1]);
	//end = timeGetTime();
	//DWORD diff2 = end-start;

	//printf("SPEED TEST %d %d\n",diff,diff2);

	// Use malloc() instead of new because, for some unknown reason, GCC 4.9 has a bug
	// that causes a std::bad_alloc exception on certain memory allocations. Right now,
	// POLYLIST and VERT are POD-style structs, so malloc() can substitute for new
	// in this case.
	if (polylists == NULL)
	{
		polylists = (POLYLIST *)malloc(sizeof(POLYLIST)*2);
		polylist = &polylists[0];
	}
	
	if (vertLists == NULL)
	{
		vertLists = (VERT *)malloc_alignedPage(VERTLIST_SIZE * sizeof(VERT) * 2);
		vertList = vertLists;
		
		vertListCount[0] = 0;
		vertListCount[1] = 0;
	}
	
	gfx3d.state.savedDISP3DCNT.value = 0;
	gfx3d.state.fogDensityTable = MMU.ARM9_REG+0x0360;
	gfx3d.state.edgeMarkColorTable = (u16 *)(MMU.ARM9_REG+0x0330);
	
	gfx3d.render3DFrameCount = 0;
	
	makeTables();
	Render3D_Init();
}

void gfx3d_deinit()
{
	Render3D_DeInit();
	
	free(polylists);
	polylists = NULL;
	polylist = NULL;
	
	free_aligned(vertLists);
	vertLists = NULL;
	vertList = NULL;
	
	delete _clipper;
}

void gfx3d_reset()
{
	if (CurrentRenderer->GetRenderNeedsFinish())
	{
		GPU->ForceRender3DFinishAndFlush(false);
		CurrentRenderer->SetRenderNeedsFinish(false);
		GPU->GetEventHandler()->DidRender3DEnd();
	}
	
#ifdef _SHOW_VTX_COUNTERS
	max_polys = max_verts = 0;
#endif

	reconstruct(&gfx3d);
	delete viewer3d_state;
	viewer3d_state = new Viewer3d_State();
	
	gxf_hardware.reset();

	drawPending = FALSE;
	memset(polylists, 0, sizeof(POLYLIST)*2);
	memset(vertLists, 0, VERTLIST_SIZE * sizeof(VERT) * 2);
	gfx3d.state.invalidateToon = true;
	listTwiddle = 1;
	twiddleLists();
	gfx3d.polylist = polylist;
	gfx3d.vertList = vertList;
	gfx3d.vertListCount = vertListCount[listTwiddle];
	gfx3d.clippedPolyCount = 0;
	gfx3d.clippedPolyOpaqueCount = 0;
	gfx3d.clippedPolyList = _clippedPolySortedList;

	polyAttrInProcess.value = 0;
	currentPolyAttr.value = 0;
	currentPolyTexParam.value = 0;
	currentPolyTexPalette = 0;
	mode = MATRIXMODE_PROJECTION;
	s16coord[0] = s16coord[1] = s16coord[2] = s16coord[3] = 0;
	coordind = 0;
	vtxFormat = GFX3D_TRIANGLES;
	memset(trans, 0, sizeof(trans));
	transind = 0;
	memset(scale, 0, sizeof(scale));
	scaleind = 0;
	viewport = 0;
	memset(gxPIPE.cmd, 0, sizeof(gxPIPE.cmd));
	memset(gxPIPE.param, 0, sizeof(gxPIPE.param));
	memset(colorRGB, 0, sizeof(colorRGB));
	memset(&tempVertInfo, 0, sizeof(tempVertInfo));
	memset(_gfx3d_savestateBuffer, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u32));

	MatrixInit(mtxCurrent[MATRIXMODE_PROJECTION]);
	MatrixInit(mtxCurrent[MATRIXMODE_POSITION]);
	MatrixInit(mtxCurrent[MATRIXMODE_POSITION_VECTOR]);
	MatrixInit(mtxCurrent[MATRIXMODE_TEXTURE]);
	MatrixInit(mtxTemporal);

	MatrixStackInit(&mtxStackProjection);
	MatrixStackInit(&mtxStackPosition);
	MatrixStackInit(&mtxStackPositionVector);
	MatrixStackInit(&mtxStackTexture);

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
	viewport = 0xBFFF0000;
	
	clInd2 = 0;
	isSwapBuffers = FALSE;

	GFX_PIPEclear();
	GFX_FIFOclear();
	
	gfx3d.render3DFrameCount = 0;
	
	CurrentRenderer->Reset();
}

//================================================================================= Geometry Engine
//=================================================================================
//=================================================================================

inline float vec3dot(float* a, float* b) {
	return (((a[0]) * (b[0])) + ((a[1]) * (b[1])) + ((a[2]) * (b[2])));
}

FORCEINLINE s32 mul_fixed32(s32 a, s32 b)
{
	return sfx32_shiftdown(fx32_mul(a,b));
}

FORCEINLINE s32 vec3dot_fixed32(const s32* a, const s32* b) {
	return sfx32_shiftdown(fx32_mul(a[0],b[0]) + fx32_mul(a[1],b[1]) + fx32_mul(a[2],b[2]));
}

//---------------
//I'm going to start name these functions GE for GEOMETRY ENGINE MATH.
//Pretty much any math function in this file should be explicit about how it's handling precision.
//Handling that stuff generically globally is not a winning proposition.

FORCEINLINE s64 GEM_Mul32x16To64(const s32 a, const s16 b)
{
	return ((s64)a)*((s64)b);
}

FORCEINLINE s64 GEM_Mul32x32To64(const s32 a, const s32 b)
{
#ifdef _MSC_VER
	return __emul(a,b);
#else
	return ((s64)a)*((s64)b);
#endif
}

static s32 GEM_SaturateAndShiftdown36To32(const s64 val)
{
	if(val>(s64)0x000007FFFFFFFFFFULL) return (s32)0x7FFFFFFFU;
	if(val<(s64)0xFFFFF80000000000ULL) return (s32)0x80000000U;

	return fx32_shiftdown(val);
}

static void GEM_TransformVertex(const s32 (&__restrict mtx)[16], s32 (&__restrict vec)[4])
{
	const s32 x = vec[0];
	const s32 y = vec[1];
	const s32 z = vec[2];
	const s32 w = vec[3];

	//saturation logic is most carefully tested by:
	//+ spectrobes beyond the portals excavation blower and drill tools: sets very large overflowing +x,+y in the modelview matrix to push things offscreen
	//You can see this happening quite clearly: vertices will get translated to extreme values and overflow from a 7FFF-like to an 8000-like
	//but if it's done wrongly, you can get bugs in:
	//+ kingdom hearts re-coded: first conversation with cast characters will place them oddly with something overflowing to about 0xA???????
	
	//other test cases that cropped up during this development, but are probably not actually related to this after all
	//+ SM64: outside castle skybox
	//+ NSMB: mario head screen wipe

	vec[0] = GEM_SaturateAndShiftdown36To32( GEM_Mul32x32To64(x,mtx[0]) + GEM_Mul32x32To64(y,mtx[4]) + GEM_Mul32x32To64(z,mtx[ 8]) + GEM_Mul32x32To64(w,mtx[12]) );
	vec[1] = GEM_SaturateAndShiftdown36To32( GEM_Mul32x32To64(x,mtx[1]) + GEM_Mul32x32To64(y,mtx[5]) + GEM_Mul32x32To64(z,mtx[ 9]) + GEM_Mul32x32To64(w,mtx[13]) );
	vec[2] = GEM_SaturateAndShiftdown36To32( GEM_Mul32x32To64(x,mtx[2]) + GEM_Mul32x32To64(y,mtx[6]) + GEM_Mul32x32To64(z,mtx[10]) + GEM_Mul32x32To64(w,mtx[14]) );
	vec[3] = GEM_SaturateAndShiftdown36To32( GEM_Mul32x32To64(x,mtx[3]) + GEM_Mul32x32To64(y,mtx[7]) + GEM_Mul32x32To64(z,mtx[11]) + GEM_Mul32x32To64(w,mtx[15]) );
}
//---------------


#define SUBMITVERTEX(ii, nn) polylist->list[polylist->count].vertIndexes[ii] = tempVertInfo.map[nn];
//Submit a vertex to the GE
static void SetVertex()
{
	s32 coord[3] = {
		s16coord[0],
		s16coord[1],
		s16coord[2]
	};

	DS_ALIGN(16) s32 coordTransformed[4] = { coord[0], coord[1], coord[2], (1<<12) };

	if (texCoordTransformMode == TextureTransformationMode_VertexSource)
	{
		//Tested by: Eledees The Adventures of Kai and Zero (E) [title screen and frontend menus]
		const s32 *mtxTex = mtxCurrent[MATRIXMODE_TEXTURE];
		last_s = (s32)( (s64)(GEM_Mul32x16To64(mtxTex[0], s16coord[0]) + GEM_Mul32x16To64(mtxTex[4], s16coord[1]) + GEM_Mul32x16To64(mtxTex[8], s16coord[2]) + ((s64)_s << 24)) >> 24 );
		last_t = (s32)( (s64)(GEM_Mul32x16To64(mtxTex[1], s16coord[0]) + GEM_Mul32x16To64(mtxTex[5], s16coord[1]) + GEM_Mul32x16To64(mtxTex[9], s16coord[2]) + ((s64)_t << 24)) >> 24 );
	}

	//refuse to do anything if we have too many verts or polys
	polygonListCompleted = 0;
	if(vertListCount[listTwiddle] >= VERTLIST_SIZE)
			return;
	if(polylist->count >= POLYLIST_SIZE) 
			return;

	if(freelookMode == 2)
	{
		//adjust projection
		s32 tmp[16];
		MatrixCopy(tmp,mtxCurrent[MATRIXMODE_PROJECTION]);
		MatrixMultiply(tmp, freelookMatrix);
		GEM_TransformVertex(mtxCurrent[MATRIXMODE_POSITION], coordTransformed); //modelview
		GEM_TransformVertex(tmp, coordTransformed); //projection
	}
	else if(freelookMode == 3)
	{
		//use provided projection
		GEM_TransformVertex(mtxCurrent[MATRIXMODE_POSITION], coordTransformed); //modelview
		GEM_TransformVertex(freelookMatrix, coordTransformed); //projection
	}
	else
	{
		//no freelook
		GEM_TransformVertex(mtxCurrent[MATRIXMODE_POSITION], coordTransformed); //modelview
		GEM_TransformVertex(mtxCurrent[MATRIXMODE_PROJECTION], coordTransformed); //projection
	}

	//TODO - culling should be done here.
	//TODO - viewport transform?

	int continuation = 0;
	if (vtxFormat==GFX3D_TRIANGLE_STRIP && !tempVertInfo.first)
		continuation = 2;
	else if (vtxFormat==GFX3D_QUAD_STRIP && !tempVertInfo.first)
		continuation = 2;

	//record the vertex
	//VERT &vert = tempVertList.list[tempVertList.count];
	const size_t vertIndex = vertListCount[listTwiddle] + tempVertInfo.count - continuation;
	if (vertIndex >= VERTLIST_SIZE)
	{
		printf("wtf\n");
	}
	
	VERT &vert = vertList[vertIndex];

	//printf("%f %f %f\n",coordTransformed[0],coordTransformed[1],coordTransformed[2]);
	//if(coordTransformed[1] > 20) 
	//	coordTransformed[1] = 20;

	//printf("y-> %f\n",coord[1]);

	//if(mtxCurrent[1][14]>15) {
	//	printf("ACK!\n");
	//	printf("----> modelview 1 state for that ack:\n");
	//	//MatrixPrint(mtxCurrent[1]);
	//}

	vert.texcoord[0] = last_s/16.0f;
	vert.texcoord[1] = last_t/16.0f;
	vert.coord[0] = coordTransformed[0]/4096.0f;
	vert.coord[1] = coordTransformed[1]/4096.0f;
	vert.coord[2] = coordTransformed[2]/4096.0f;
	vert.coord[3] = coordTransformed[3]/4096.0f;
	vert.color[0] = GFX3D_5TO6_LOOKUP(colorRGB[0]);
	vert.color[1] = GFX3D_5TO6_LOOKUP(colorRGB[1]);
	vert.color[2] = GFX3D_5TO6_LOOKUP(colorRGB[2]);
	vert.color_to_float();
	tempVertInfo.map[tempVertInfo.count] = vertListCount[listTwiddle] + tempVertInfo.count - continuation;
	tempVertInfo.count++;

	//possibly complete a polygon
	{
		polygonListCompleted = 2;
		switch(vtxFormat)
		{
			case GFX3D_TRIANGLES:
				if(tempVertInfo.count!=3)
					break;
				polygonListCompleted = 1;
				//vertlist->list[polylist->list[polylist->count].vertIndexes[i] = vertlist->count++] = tempVertList.list[n];
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,2);
				vertListCount[listTwiddle] += 3;
				polylist->list[polylist->count].type = POLYGON_TYPE_TRIANGLE;
				tempVertInfo.count = 0;
				break;
				
			case GFX3D_QUADS:
				if(tempVertInfo.count!=4)
					break;
				polygonListCompleted = 1;
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,2);
				SUBMITVERTEX(3,3);
				vertListCount[listTwiddle] += 4;
				polylist->list[polylist->count].type = POLYGON_TYPE_QUAD;
				tempVertInfo.count = 0;
				break;
				
			case GFX3D_TRIANGLE_STRIP:
				if(tempVertInfo.count!=3)
					break;
				polygonListCompleted = 1;
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,2);
				polylist->list[polylist->count].type = POLYGON_TYPE_TRIANGLE;

				if(triStripToggle)
					tempVertInfo.map[1] = vertListCount[listTwiddle]+2-continuation;
				else
					tempVertInfo.map[0] = vertListCount[listTwiddle]+2-continuation;
				
				if(tempVertInfo.first)
					vertListCount[listTwiddle] += 3;
				else
					vertListCount[listTwiddle] += 1;

				triStripToggle ^= 1;
				tempVertInfo.first = false;
				tempVertInfo.count = 2;
				break;
				
			case GFX3D_QUAD_STRIP:
				if(tempVertInfo.count!=4)
					break;
				polygonListCompleted = 1;
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,3);
				SUBMITVERTEX(3,2);
				polylist->list[polylist->count].type = POLYGON_TYPE_QUAD;
				tempVertInfo.map[0] = vertListCount[listTwiddle]+2-continuation;
				tempVertInfo.map[1] = vertListCount[listTwiddle]+3-continuation;
				if(tempVertInfo.first)
					vertListCount[listTwiddle] += 4;
				else vertListCount[listTwiddle] += 2;
				tempVertInfo.first = false;
				tempVertInfo.count = 2;
				break;
				
			default: 
				return;
		}

		if (polygonListCompleted == 1)
		{
			POLY &poly = polylist->list[polylist->count];
			
			poly.vtxFormat = vtxFormat;

			// Line segment detect
			// Tested" Castlevania POR - warp stone, trajectory of ricochet, "Eye of Decay"
			if (currentPolyTexParam.PackedFormat == TEXMODE_NONE)
			{
				bool duplicated = false;
				const VERT &vert0 = vertList[poly.vertIndexes[0]];
				const VERT &vert1 = vertList[poly.vertIndexes[1]];
				const VERT &vert2 = vertList[poly.vertIndexes[2]];
				if ( (vert0.x == vert1.x) && (vert0.y == vert1.y) ) duplicated = true;
				else
					if ( (vert1.x == vert2.x) && (vert1.y == vert2.y) ) duplicated = true;
					else
						if ( (vert0.y == vert1.y) && (vert1.y == vert2.y) ) duplicated = true;
						else
							if ( (vert0.x == vert1.x) && (vert1.x == vert2.x) ) duplicated = true;
				if (duplicated)
				{
					//printf("Line Segmet detected (poly type %i, mode %i, texparam %08X)\n", poly.type, poly.vtxFormat, textureFormat);
					poly.vtxFormat = (PolygonPrimitiveType)(vtxFormat + 4);
				}
			}

			poly.attribute = polyAttrInProcess;
			poly.texParam = currentPolyTexParam;
			poly.texPalette = currentPolyTexPalette;
			poly.viewport = viewport;
			polylist->count++;
		}
	}
}

static void UpdateProjection()
{
#ifdef HAVE_LUA
	if(freelookMode == 0) return;
	float floatproj[16];
	for(int i=0;i<16;i++)
		floatproj[i] = mtxCurrent[MATRIXMODE_PROJECTION][i]/((float)(1<<12));
	CallRegistered3dEvent(0,floatproj);
#endif
}

static void gfx3d_glPolygonAttrib_cache()
{
	// Light enable/disable
	lightMask = polyAttrInProcess.LightMask;
}

static void gfx3d_glTexImage_cache()
{
	texCoordTransformMode = (TextureTransformationMode)currentPolyTexParam.TexCoordTransformMode;
}

static void gfx3d_glLightDirection_cache(const size_t index)
{
	s32 v = lightDirection[index];

	s16 x = ((v<<22)>>22)<<3;
	s16 y = ((v<<12)>>22)<<3;
	s16 z = ((v<<2)>>22)<<3;

	cacheLightDirection[index][0] = x;
	cacheLightDirection[index][1] = y;
	cacheLightDirection[index][2] = z;
	cacheLightDirection[index][3] = 0;

	//Multiply the vector by the directional matrix
	MatrixMultVec3x3(mtxCurrent[MATRIXMODE_POSITION_VECTOR], cacheLightDirection[index]);

	//Calculate the half angle vector
	s32 lineOfSight[4] = {0, 0, (-1)<<12, 0};
	for (size_t i = 0; i < 4; i++)
	{
		cacheHalfVector[index][i] = ((cacheLightDirection[index][i] + lineOfSight[i]));
	}

	//normalize the half angle vector
	//can't believe the hardware really does this... but yet it seems...
	s32 halfLength = ((s32)(sqrt((double)vec3dot_fixed32(cacheHalfVector[index],cacheHalfVector[index]))))<<6;

	if (halfLength != 0)
	{
		halfLength = abs(halfLength);
		halfLength >>= 6;
		for (size_t i = 0; i < 4; i++)
		{
			s32 temp = cacheHalfVector[index][i];
			temp <<= 6;
			temp /= halfLength;
			cacheHalfVector[index][i] = temp;
		}
	}
}


//===============================================================================
static void gfx3d_glMatrixMode(u32 v)
{
	mode = (MatrixMode)(v & 0x03);

	GFX_DELAY(1);
}

static void gfx3d_glPushMatrix()
{
	//1. apply offset specified by push (well, it's always +1) to internal counter
	//2. mask that bit off to actually index the matrix for reading
	//3. SE is set depending on resulting internal counter

	//printf("%d %d %d %d -> ",mtxStack[0].position,mtxStack[1].position,mtxStack[2].position,mtxStack[3].position);
	//printf("PUSH mode: %d -> ",mode,mtxStack[mode].position);

	if (mode == MATRIXMODE_PROJECTION || mode == MATRIXMODE_TEXTURE)
	{
		if (mode == MATRIXMODE_PROJECTION)
		{
			MatrixCopy(mtxStackProjection.matrix[0], mtxCurrent[mode]);
			
			u32 &index = mtxStackProjection.position;
			if (index == 1) MMU_new.gxstat.se = 1;
			index += 1;
			index &= 1;

			UpdateProjection();
		}
		else
		{
			MatrixCopy(mtxStackTexture.matrix[0], mtxCurrent[mode]);
			
			u32 &index = mtxStackTexture.position;
			if (index == 1) MMU_new.gxstat.se = 1; //unknown if this applies to the texture matrix
			index += 1;
			index &= 1;
		}
	}
	else
	{
		u32 &index = mtxStackPosition.position;
		
		MatrixCopy(mtxStackPosition.matrix[index & 31], mtxCurrent[MATRIXMODE_POSITION]);
		MatrixCopy(mtxStackPositionVector.matrix[index & 31], mtxCurrent[MATRIXMODE_POSITION_VECTOR]);
		
		index += 1;
		index &= 63;
		if (index >= 32) MMU_new.gxstat.se = 1; //(not sure, this might be off by 1)
	}

	//printf("%d %d %d %d\n",mtxStack[0].position,mtxStack[1].position,mtxStack[2].position,mtxStack[3].position);

	GFX_DELAY(17);
}

static void gfx3d_glPopMatrix(u32 v)
{
	//1. apply offset specified by pop to internal counter
	//2. SE is set depending on resulting internal counter
	//3. mask that bit off to actually index the matrix for reading

	//printf("%d %d %d %d -> ",mtxStack[0].position,mtxStack[1].position,mtxStack[2].position,mtxStack[3].position);
	//printf("POP   (%d): mode: %d -> ",v,mode,mtxStack[mode].position);

	if (mode == MATRIXMODE_PROJECTION || mode == MATRIXMODE_TEXTURE)
	{
		//parameter is ignored and treated as sensible (always 1)
		
		if (mode == MATRIXMODE_PROJECTION)
		{
			u32 &index = mtxStackProjection.position;
			index ^= 1;
			if (index == 1) MMU_new.gxstat.se = 1;
			MatrixCopy(mtxCurrent[mode], mtxStackProjection.matrix[0]);

			UpdateProjection();
		}
		else
		{
			u32 &index = mtxStackTexture.position;
			index ^= 1;
			if (index == 1) MMU_new.gxstat.se = 1; //unknown if this applies to the texture matrix
			MatrixCopy(mtxCurrent[mode], mtxStackTexture.matrix[0]);
		}
	}
	else
	{
		u32 &index = mtxStackPosition.position;
			
		index -= v & 63;
		index &= 63;
		if (index >= 32) MMU_new.gxstat.se = 1; //(not sure, this might be off by 1)
		
		MatrixCopy(mtxCurrent[MATRIXMODE_POSITION], mtxStackPosition.matrix[index & 31]);
		MatrixCopy(mtxCurrent[MATRIXMODE_POSITION_VECTOR], mtxStackPositionVector.matrix[index & 31]);
	}

	//printf("%d %d %d %d\n",mtxStack[0].position,mtxStack[1].position,mtxStack[2].position,mtxStack[3].position);

	GFX_DELAY(36);
}

static void gfx3d_glStoreMatrix(u32 v)
{
	//printf("%d %d %d %d -> ",mtxStack[0].position,mtxStack[1].position,mtxStack[2].position,mtxStack[3].position);
	//printf("STORE (%d): mode: %d -> ",v,mode,mtxStack[mode].position);

	if (mode == MATRIXMODE_PROJECTION || mode == MATRIXMODE_TEXTURE)
	{
		//parameter ignored and treated as sensible
		v = 0;
		
		if (mode == MATRIXMODE_PROJECTION)
		{
			MatrixCopy(mtxStackProjection.matrix[0], mtxCurrent[MATRIXMODE_PROJECTION]);
			UpdateProjection();
		}
		else
		{
			MatrixCopy(mtxStackTexture.matrix[0], mtxCurrent[MATRIXMODE_TEXTURE]);
		}
	}
	else
	{
		v &= 31;

		//out of bounds function fully properly, but set errors (not sure, this might be off by 1)
		if (v >= 31) MMU_new.gxstat.se = 1;
		
		MatrixCopy(mtxStackPosition.matrix[v], mtxCurrent[MATRIXMODE_POSITION]);
		MatrixCopy(mtxStackPositionVector.matrix[v], mtxCurrent[MATRIXMODE_POSITION_VECTOR]);
	}

	//printf("%d %d %d %d\n",mtxStack[0].position,mtxStack[1].position,mtxStack[2].position,mtxStack[3].position);

	GFX_DELAY(17);
}

static void gfx3d_glRestoreMatrix(u32 v)
{
	if (mode == MATRIXMODE_PROJECTION || mode == MATRIXMODE_TEXTURE)
	{
		//parameter ignored and treated as sensible
		v = 0;
		
		if (mode == MATRIXMODE_PROJECTION)
		{
			MatrixCopy(mtxCurrent[MATRIXMODE_PROJECTION], mtxStackProjection.matrix[0]);
			UpdateProjection();
		}
		else
		{
			MatrixCopy(mtxCurrent[MATRIXMODE_TEXTURE], mtxStackTexture.matrix[0]);
		}
	}
	else
	{
		//out of bounds errors function fully properly, but set errors
		MMU_new.gxstat.se = (v >= 31) ? 1 : 0; //(not sure, this might be off by 1)
		
		MatrixCopy(mtxCurrent[MATRIXMODE_POSITION], mtxStackPosition.matrix[v]);
		MatrixCopy(mtxCurrent[MATRIXMODE_POSITION_VECTOR], mtxStackPositionVector.matrix[v]);
	}


	GFX_DELAY(36);
}

static void gfx3d_glLoadIdentity()
{
	MatrixIdentity(mtxCurrent[mode]);

	GFX_DELAY(19);

	if (mode == MATRIXMODE_POSITION_VECTOR)
		MatrixIdentity(mtxCurrent[MATRIXMODE_POSITION]);

	//printf("identity: %d to: \n",mode); MatrixPrint(mtxCurrent[1]);
}

static BOOL gfx3d_glLoadMatrix4x4(s32 v)
{
	mtxCurrent[mode][ML4x4ind] = v;

	++ML4x4ind;
	if(ML4x4ind<16) return FALSE;
	ML4x4ind = 0;

	GFX_DELAY(19);

	//vector_fix2float<4>(mtxCurrent[mode], 4096.f);

	if (mode == MATRIXMODE_POSITION_VECTOR)
		MatrixCopy(mtxCurrent[MATRIXMODE_POSITION], mtxCurrent[MATRIXMODE_POSITION_VECTOR]);

	//printf("load4x4: matrix %d to: \n",mode); MatrixPrint(mtxCurrent[1]);
	return TRUE;
}

static BOOL gfx3d_glLoadMatrix4x3(s32 v)
{
	mtxCurrent[mode][ML4x3ind] = v;

	ML4x3ind++;
	if((ML4x3ind & 0x03) == 3) ML4x3ind++;
	if(ML4x3ind<16) return FALSE;
	ML4x3ind = 0;

	//vector_fix2float<4>(mtxCurrent[mode], 4096.f);

	//fill in the unusued matrix values
	mtxCurrent[mode][3] = mtxCurrent[mode][7] = mtxCurrent[mode][11] = 0;
	mtxCurrent[mode][15] = (1<<12);

	GFX_DELAY(30);

	if (mode == MATRIXMODE_POSITION_VECTOR)
		MatrixCopy(mtxCurrent[MATRIXMODE_POSITION], mtxCurrent[MATRIXMODE_POSITION_VECTOR]);
	//printf("load4x3: matrix %d to: \n",mode); MatrixPrint(mtxCurrent[1]);
	return TRUE;
}

static BOOL gfx3d_glMultMatrix4x4(s32 v)
{
	mtxTemporal[MM4x4ind] = v;

	MM4x4ind++;
	if(MM4x4ind<16) return FALSE;
	MM4x4ind = 0;

	GFX_DELAY(35);

	//vector_fix2float<4>(mtxTemporal, 4096.f);

	MatrixMultiply(mtxCurrent[mode], mtxTemporal);

	if (mode == MATRIXMODE_POSITION_VECTOR)
	{
		MatrixMultiply(mtxCurrent[MATRIXMODE_POSITION], mtxTemporal);
		GFX_DELAY_M2(30);
	}

	if(mode == MATRIXMODE_PROJECTION)
	{
		UpdateProjection();
	}

	//printf("mult4x4: matrix %d to: \n",mode); MatrixPrint(mtxCurrent[1]);

	MatrixIdentity(mtxTemporal);
	return TRUE;
}

static BOOL gfx3d_glMultMatrix4x3(s32 v)
{
	mtxTemporal[MM4x3ind] = v;

	MM4x3ind++;
	if ((MM4x3ind & 0x03) == 3) MM4x3ind++;
	if (MM4x3ind < 16) return FALSE;
	MM4x3ind = 0;

	GFX_DELAY(31);

	//vector_fix2float<4>(mtxTemporal, 4096.f);

	//fill in the unusued matrix values
	mtxTemporal[3] = mtxTemporal[7] = mtxTemporal[11] = 0;
	mtxTemporal[15] = 1 << 12;

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);

	if (mode == MATRIXMODE_POSITION_VECTOR)
	{
		MatrixMultiply (mtxCurrent[MATRIXMODE_POSITION], mtxTemporal);
		GFX_DELAY_M2(30);
	}

	if(mode == MATRIXMODE_PROJECTION)
	{
		UpdateProjection();
	}

	//printf("mult4x3: matrix %d to: \n",mode); MatrixPrint(mtxCurrent[1]);

	//does this really need to be done?
	MatrixIdentity(mtxTemporal);
	return TRUE;
}

static BOOL gfx3d_glMultMatrix3x3(s32 v)
{
	mtxTemporal[MM3x3ind] = v;
	
	MM3x3ind++;
	if ((MM3x3ind & 0x03) == 3) MM3x3ind++;
	if (MM3x3ind<12) return FALSE;
	MM3x3ind = 0;

	GFX_DELAY(28);

	//vector_fix2float<3>(mtxTemporal, 4096.f);

	//fill in the unusued matrix values
	mtxTemporal[3] = mtxTemporal[7] = mtxTemporal[11] = 0;
	mtxTemporal[15] = 1<<12;
	mtxTemporal[12] = mtxTemporal[13] = mtxTemporal[14] = 0;

	MatrixMultiply(mtxCurrent[mode], mtxTemporal);

	if (mode == MATRIXMODE_POSITION_VECTOR)
	{
		MatrixMultiply(mtxCurrent[MATRIXMODE_POSITION], mtxTemporal);
		GFX_DELAY_M2(30);
	}

	if(mode == MATRIXMODE_PROJECTION)
	{
		UpdateProjection();
	}

	//printf("mult3x3: matrix %d to: \n",mode); MatrixPrint(mtxCurrent[1]);


	//does this really need to be done?
	MatrixIdentity(mtxTemporal);
	return TRUE;
}

static BOOL gfx3d_glScale(s32 v)
{
	scale[scaleind] = v;

	++scaleind;

	if (scaleind < 3) return FALSE;
	scaleind = 0;

	MatrixScale(mtxCurrent[(mode == MATRIXMODE_POSITION_VECTOR ? MATRIXMODE_POSITION : mode)], scale);
	//printf("scale: matrix %d to: \n",mode); MatrixPrint(mtxCurrent[1]);

	GFX_DELAY(22);

	//note: pos-vector mode should not cause both matrices to scale.
	//the whole purpose is to keep the vector matrix orthogonal
	//so, I am leaving this commented out as an example of what not to do.
	//if (mode == MATRIXMODE_POSITION_VECTOR)
	//	MatrixScale (mtxCurrent[MATRIXMODE_POSITION], scale);
	return TRUE;
}

static BOOL gfx3d_glTranslate(s32 v)
{
	trans[transind] = v;

	++transind;

	if (transind < 3) return FALSE;
	transind = 0;

	MatrixTranslate(mtxCurrent[mode], trans);

	GFX_DELAY(22);

	if (mode == MATRIXMODE_POSITION_VECTOR)
	{
		MatrixTranslate(mtxCurrent[MATRIXMODE_POSITION], trans);
		GFX_DELAY_M2(30);
	}

	//printf("translate: matrix %d to: \n",mode); MatrixPrint(mtxCurrent[1]);

	return TRUE;
}

static void gfx3d_glColor3b(u32 v)
{
	colorRGB[0] = (v&0x1F);
	colorRGB[1] = ((v>>5)&0x1F);
	colorRGB[2] = ((v>>10)&0x1F);
	GFX_DELAY(1);
}

static void gfx3d_glNormal(s32 v)
{
	s16 nx = ((v<<22)>>22)<<3;
	s16 ny = ((v<<12)>>22)<<3;
	s16 nz = ((v<<2)>>22)<<3;

	CACHE_ALIGN s32 normal[4] =  { nx,ny,nz,(1<<12) };

	if (texCoordTransformMode == TextureTransformationMode_NormalSource)
	{
		//SM64 highlight rendered star in main menu tests this
		//also smackdown 2010 player textures tested this (needed cast on _s and _t)
		const s32 *mtxTex = mtxCurrent[MATRIXMODE_TEXTURE];
		last_s = (s32)( (s64)(GEM_Mul32x32To64(mtxTex[0], normal[0]) + GEM_Mul32x32To64(mtxTex[4], normal[1]) + GEM_Mul32x32To64(mtxTex[8], normal[2]) + ((s64)_s << 24)) >> 24 );
		last_t = (s32)( (s64)(GEM_Mul32x32To64(mtxTex[1], normal[0]) + GEM_Mul32x32To64(mtxTex[5], normal[1]) + GEM_Mul32x32To64(mtxTex[9], normal[2]) + ((s64)_t << 24)) >> 24 );
	}

	MatrixMultVec3x3(mtxCurrent[MATRIXMODE_POSITION_VECTOR], normal);

	//apply lighting model
	u8 diffuse[3] = {
		(u8)( dsDiffuse        & 0x1F),
		(u8)((dsDiffuse >>  5) & 0x1F),
		(u8)((dsDiffuse >> 10) & 0x1F) };

	u8 ambient[3] = {
		(u8)( dsAmbient        & 0x1F),
		(u8)((dsAmbient >>  5) & 0x1F),
		(u8)((dsAmbient >> 10) & 0x1F) };

	u8 emission[3] = {
		(u8)( dsEmission        & 0x1F),
		(u8)((dsEmission >>  5) & 0x1F),
		(u8)((dsEmission >> 10) & 0x1F) };

	u8 specular[3] = {
		(u8)( dsSpecular        & 0x1F),
		(u8)((dsSpecular >>  5) & 0x1F),
		(u8)((dsSpecular >> 10) & 0x1F) };

	int vertexColor[3] = { emission[0], emission[1], emission[2] };

	for (size_t i = 0; i < 4; i++)
	{
		if (!((lightMask>>i)&1)) continue;

		u8 _lightColor[3] = {
			(u8)( lightColor[i]        & 0x1F),
			(u8)((lightColor[i] >> 5)  & 0x1F),
			(u8)((lightColor[i] >> 10) & 0x1F) };

		//This formula is the one used by the DS
		//Reference : http://nocash.emubase.de/gbatek.htm#ds3dpolygonlightparameters
		s32 fixed_diffuse = std::max(0,-vec3dot_fixed32(cacheLightDirection[i],normal));
		
		//todo - this could be cached in this form
		s32 fixedTempNegativeHalf[] = {-cacheHalfVector[i][0],-cacheHalfVector[i][1],-cacheHalfVector[i][2],-cacheHalfVector[i][3]};
		s32 dot = vec3dot_fixed32(fixedTempNegativeHalf, normal);

		s32 fixedshininess = 0;
		if (dot > 0) //prevent shininess on opposite side
		{
			//we have cos(a). it seems that we need cos(2a). trig identity is a fast way to get it.
			//cos^2(a)=(1/2)(1+cos(2a))
			//2*cos^2(a)-1=cos(2a)
			fixedshininess = 2*mul_fixed32(dot,dot)-4096;
			//gbatek is almost right but not quite!
		}

		//this seems to need to be saturated, or else the table will overflow.
		//even without a table, failure to saturate is bad news
		fixedshininess = std::min(fixedshininess,4095);
		fixedshininess = std::max(fixedshininess,0);
		
		if (dsSpecular & 0x8000)
		{
			//shininess is 20.12 fixed point, so >>5 gives us .7 which is 128 entries
			//the entries are 8bits each so <<4 gives us .12 again, compatible with the lighting formulas below
			//(according to other normal nds procedures, we might should fill the bottom bits with 1 or 0 according to rules...)
			fixedshininess = gfx3d.state.shininessTable[fixedshininess>>5]<<4;
		}

		for (size_t c = 0; c < 3; c++)
		{
			s32 specComp = ((specular[c] * _lightColor[c] * fixedshininess)>>17);  //5 bits for color*color and 12 bits for the shininess
			s32 diffComp = ((diffuse[c] * _lightColor[c] * fixed_diffuse)>>17); //5bits for the color*color and 12 its for the diffuse
			s32 ambComp = ((ambient[c] * _lightColor[c])>>5); //5bits for color*color
			vertexColor[c] += specComp + diffComp + ambComp;
		}
	}

	for (size_t c = 0; c < 3; c++)
	{
		colorRGB[c] = std::min(31,vertexColor[c]);
	}

	GFX_DELAY(9);
	GFX_DELAY_M2((lightMask) & 0x01);
	GFX_DELAY_M2((lightMask>>1) & 0x01);
	GFX_DELAY_M2((lightMask>>2) & 0x01);
	GFX_DELAY_M2((lightMask>>3) & 0x01);
}

static void gfx3d_glTexCoord(s32 val)
{
	_s = ((val<<16)>>16);
	_t = (val>>16);

	if (texCoordTransformMode == TextureTransformationMode_TexCoordSource)
	{
		//dragon quest 4 overworld will test this
		const s32 *mtxTex = mtxCurrent[MATRIXMODE_TEXTURE];
		last_s = (s32)( (s64)(GEM_Mul32x32To64(mtxTex[0], _s) + GEM_Mul32x32To64(mtxTex[4], _t) + (s64)mtxTex[8] + (s64)mtxTex[12]) >> 12 );
		last_t = (s32)( (s64)(GEM_Mul32x32To64(mtxTex[1], _s) + GEM_Mul32x32To64(mtxTex[5], _t) + (s64)mtxTex[9] + (s64)mtxTex[13]) >> 12 );
	}
	else if (texCoordTransformMode == TextureTransformationMode_None)
	{
		last_s=_s;
		last_t=_t;
	}
	GFX_DELAY(1);
}

static BOOL gfx3d_glVertex16b(s32 v)
{
	if (coordind == 0)
	{
		s16coord[0] = (v<<16)>>16;
		s16coord[1] = (v>>16)&0xFFFF;

		++coordind;
		return FALSE;
	}

	s16coord[2] = (v<<16)>>16;

	coordind = 0;
	SetVertex ();

	GFX_DELAY(9);
	return TRUE;
}

static void gfx3d_glVertex10b(s32 v)
{
	//TODO TODO TODO - contemplate the sign extension - shift in zeroes or ones? zeroes is certainly more normal..
	s16coord[0] = ((v<<22)>>22)<<6;
	s16coord[1] = ((v<<12)>>22)<<6;
	s16coord[2] = ((v<<2)>>22)<<6;

	GFX_DELAY(8);
	SetVertex ();
}

template<int ONE, int TWO>
static void gfx3d_glVertex3_cord(s32 v)
{
	s16coord[ONE]		= (v<<16)>>16;
	s16coord[TWO]		= (v>>16);

	SetVertex ();

	GFX_DELAY(8);
}

static void gfx3d_glVertex_rel(s32 v)
{
	s16 x = ((v<<22)>>22);
	s16 y = ((v<<12)>>22);
	s16 z = ((v<<2)>>22);

	s16coord[0] += x;
	s16coord[1] += y;
	s16coord[2] += z;


	SetVertex ();

	GFX_DELAY(8);
}

static void gfx3d_glPolygonAttrib (u32 val)
{
	if(inBegin) {
		//PROGINFO("Set polyattr in the middle of a begin/end pair.\n  (This won't be activated until the next begin)\n");
		//TODO - we need some some similar checking for teximageparam etc.
	}
	currentPolyAttr.value = val;
	GFX_DELAY(1);
}

static void gfx3d_glTexImage(u32 val)
{
	currentPolyTexParam.value = val;
	gfx3d_glTexImage_cache();
	GFX_DELAY(1);
}

static void gfx3d_glTexPalette(u32 val)
{
	currentPolyTexPalette = val;
	GFX_DELAY(1);
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
static void gfx3d_glMaterial0(u32 val)
{
	dsDiffuse = val&0xFFFF;
	dsAmbient = val>>16;

	if (BIT15(val))
	{
		colorRGB[0] = (val)&0x1F;
		colorRGB[1] = (val>>5)&0x1F;
		colorRGB[2] = (val>>10)&0x1F;
	}
	GFX_DELAY(4);
}

static void gfx3d_glMaterial1(u32 val)
{
	dsSpecular = val&0xFFFF;
	dsEmission = val>>16;
	GFX_DELAY(4);
}

/*
	0-9   Directional Vector's X component (1bit sign + 9bit fractional part)
	10-19 Directional Vector's Y component (1bit sign + 9bit fractional part)
	20-29 Directional Vector's Z component (1bit sign + 9bit fractional part)
	30-31 Light Number                     (0..3)
*/
static void gfx3d_glLightDirection(u32 v)
{
	const size_t index = v >> 30;

	lightDirection[index] = (s32)(v & 0x3FFFFFFF);
	gfx3d_glLightDirection_cache(index);
	GFX_DELAY(6);
}

static void gfx3d_glLightColor(u32 v)
{
	const size_t index = v >> 30;
	lightColor[index] = v;
	GFX_DELAY(1);
}

static BOOL gfx3d_glShininess(u32 val)
{
	gfx3d.state.shininessTable[shininessInd++] =   (val        & 0xFF);
	gfx3d.state.shininessTable[shininessInd++] = (((val >>  8) & 0xFF));
	gfx3d.state.shininessTable[shininessInd++] = (((val >> 16) & 0xFF));
	gfx3d.state.shininessTable[shininessInd++] = (((val >> 24) & 0xFF));

	if (shininessInd < 128) return FALSE;
	shininessInd = 0;
	GFX_DELAY(32);
	return TRUE;
}

static void gfx3d_glBegin(u32 v)
{
	inBegin = TRUE;
	vtxFormat = (PolygonPrimitiveType)(v & 0x03);
	triStripToggle = 0;
	tempVertInfo.count = 0;
	tempVertInfo.first = true;
	polyAttrInProcess = currentPolyAttr;
	gfx3d_glPolygonAttrib_cache();
	GFX_DELAY(1);
}

static void gfx3d_glEnd(void)
{
	tempVertInfo.count = 0;
	inBegin = FALSE;
	GFX_DELAY(1);
}

// swap buffers - skipped

static void gfx3d_glViewPort(u32 v)
{
	viewport = v;
	GFX_DELAY(1);
}

static BOOL gfx3d_glBoxTest(u32 v)
{
	//printf("boxtest\n");

	//clear result flag. busy flag has been set by fifo component already
	MMU_new.gxstat.tr = 0;		

	BTcoords[BTind++] = v & 0xFFFF;
	BTcoords[BTind++] = v >> 16;

	if (BTind < 5) return FALSE;
	BTind = 0;

	GFX_DELAY(103);

	//now that we're executing this, we're not busy anymore
	MMU_new.gxstat.tb = 0;

#if 0
	INFO("BoxTEST: x %f y %f width %f height %f depth %f\n", 
				BTcoords[0], BTcoords[1], BTcoords[2], BTcoords[3], BTcoords[4], BTcoords[5]);
	/*for (int i = 0; i < 16; i++)
	{
		INFO("mtx1[%i] = %f ", i, mtxCurrent[1][i]);
		if ((i+1) % 4 == 0) INFO("\n");
	}
	INFO("\n");*/
#endif

	//(crafted to be clear, not fast.)

	//nanostray title, ff4, ice age 3 depend on this and work
	//garfields nightmare and strawberry shortcake DO DEPEND on the overflow behavior.

	u16 ux = BTcoords[0];
	u16 uy = BTcoords[1];
	u16 uz = BTcoords[2];
	u16 uw = BTcoords[3];
	u16 uh = BTcoords[4];
	u16 ud = BTcoords[5];

	//craft the coords by adding extents to startpoint
	float x = float16table[ux];
	float y = float16table[uy];
	float z = float16table[uz];
	float xw = float16table[(ux+uw)&0xFFFF]; //&0xFFFF not necessary for u16+u16 addition but added for emphasis
	float yh = float16table[(uy+uh)&0xFFFF];
	float zd = float16table[(uz+ud)&0xFFFF];

	//eight corners of cube
	CACHE_ALIGN VERT verts[8];
	verts[0].set_coord(x,y,z,1);
	verts[1].set_coord(xw,y,z,1);
	verts[2].set_coord(xw,yh,z,1);
	verts[3].set_coord(x,yh,z,1);
	verts[4].set_coord(x,y,zd,1);
	verts[5].set_coord(xw,y,zd,1);
	verts[6].set_coord(xw,yh,zd,1);
	verts[7].set_coord(x,yh,zd,1);

	//craft the faces of the box (clockwise)
	POLY polys[6];
	polys[0].setVertIndexes(7,6,5,4); //near 
	polys[1].setVertIndexes(0,1,2,3); //far
	polys[2].setVertIndexes(0,3,7,4); //left
	polys[3].setVertIndexes(6,2,1,5); //right
	polys[4].setVertIndexes(3,2,6,7); //top
	polys[5].setVertIndexes(0,4,5,1); //bottom

	//setup the clipper
	CPoly tempClippedPoly;
	boxtestClipper.SetClippedPolyBufferPtr(&tempClippedPoly);
	boxtestClipper.Reset();

	////-----------------------------
	////awesome hack:
	////emit the box as geometry for testing
	//for(int i=0;i<6;i++) 
	//{
	//	POLY* poly = &polys[i];
	//	VERT* vertTable[4] = {
	//		&verts[poly->vertIndexes[0]],
	//		&verts[poly->vertIndexes[1]],
	//		&verts[poly->vertIndexes[2]],
	//		&verts[poly->vertIndexes[3]]
	//	};

	//	gfx3d_glBegin(1);
	//	for(int i=0;i<4;i++) {
	//		coord[0] = vertTable[i]->x;
	//		coord[1] = vertTable[i]->y;
	//		coord[2] = vertTable[i]->z;
	//		SetVertex();
	//	}
	//	gfx3d_glEnd();
	//}
	////---------------------

	//transform all coords
	for (size_t i = 0; i < 8; i++)
	{
		//this cant work. its left as a reminder that we could (and probably should) do the boxtest in all fixed point values
		//MatrixMultVec4x4_M2(mtxCurrent[0], verts[i].coord);

		//but change it all to floating point and do it that way instead

		//DS_ALIGN(16) VERT_POS4f vert = { verts[i].x, verts[i].y, verts[i].z, verts[i].w };
		
		//_MatrixMultVec4x4_NoSIMD(mtxCurrent[MATRIXMODE_POSITION], verts[i].coord);
		//_MatrixMultVec4x4_NoSIMD(mtxCurrent[MATRIXMODE_PROJECTION], verts[i].coord);
		MatrixMultVec4x4(mtxCurrent[MATRIXMODE_POSITION], verts[i].coord);
		MatrixMultVec4x4(mtxCurrent[MATRIXMODE_PROJECTION], verts[i].coord);
	}

	//clip each poly
	for (size_t i = 0; i < 6; i++)
	{
		const POLY &thePoly = polys[i];
		const VERT *vertTable[4] = {
			&verts[thePoly.vertIndexes[0]],
			&verts[thePoly.vertIndexes[1]],
			&verts[thePoly.vertIndexes[2]],
			&verts[thePoly.vertIndexes[3]]
		};

		const bool isPolyUnclipped = boxtestClipper.ClipPoly<ClipperMode_DetermineClipOnly>(0, thePoly, vertTable);
		
		//if any portion of this poly was retained, then the test passes.
		if (isPolyUnclipped)
		{
			//printf("%06d PASS %d\n",gxFIFO.size, i);
			MMU_new.gxstat.tr = 1;
			break;
		}
		else
		{
		}

		//if(i==5) printf("%06d FAIL\n",gxFIFO.size);
	}

	//printf("%06d RESULT %d\n",gxFIFO.size, MMU_new.gxstat.tr);

	return TRUE;
}

static BOOL gfx3d_glPosTest(u32 v)
{
	//this is apparently tested by transformers decepticons and ultimate spiderman

	//clear result flag. busy flag has been set by fifo component already
	MMU_new.gxstat.tr = 0;

	//now that we're executing this, we're not busy anymore
	MMU_new.gxstat.tb = 0;

	PTcoords[PTind++] = float16table[v & 0xFFFF];
	PTcoords[PTind++] = float16table[v >> 16];

	if (PTind < 3) return FALSE;
	PTind = 0;
	
	PTcoords[3] = 1.0f;
	
	MatrixMultVec4x4(mtxCurrent[MATRIXMODE_POSITION], PTcoords);
	MatrixMultVec4x4(mtxCurrent[MATRIXMODE_PROJECTION], PTcoords);

	MMU_new.gxstat.tb = 0;

	GFX_DELAY(9);

	return TRUE;
}

static void gfx3d_glVecTest(u32 v)
{
	//printf("vectest\n");
	GFX_DELAY(5);

	//this is tested by phoenix wright in its evidence inspector modelviewer
	//i am not sure exactly what it is doing, maybe it is testing to ensure
	//that the normal vector for the point of interest is camera-facing.

	CACHE_ALIGN float normal[4] = {
		normalTable[v&1023],
		normalTable[(v>>10)&1023],
		normalTable[(v>>20)&1023],
		0
	};
	
	MatrixMultVec4x4(mtxCurrent[MATRIXMODE_POSITION_VECTOR], normal);

	s16 x = (s16)(normal[0]*4096);
	s16 y = (s16)(normal[1]*4096);
	s16 z = (s16)(normal[2]*4096);

	MMU_new.gxstat.tb = 0;		// clear busy
	T1WriteWord(MMU.MMU_MEM[0][0x40], 0x630, x);
	T1WriteWord(MMU.MMU_MEM[0][0x40], 0x632, y);
	T1WriteWord(MMU.MMU_MEM[0][0x40], 0x634, z);

}
//================================================================================= Geometry Engine
//================================================================================= (end)
//=================================================================================

void VIEWPORT::decode(const u32 v)
{
	//test: homie rollerz character select chooses nonsense for Y. they did the math backwards. their goal was a fullscreen viewport, they just messed up.
	//they also messed up the width...

	u8 x1 = (v>> 0)&0xFF;
	u8 y1 = (v>> 8)&0xFF;
	u8 x2 = (v>>16)&0xFF;
	u8 y2 = (v>>24)&0xFF;

	//I'm 100% sure this is basically 99% correct
	//the modular math is right. the details of how the +1 is handled may be wrong (this might should be dealt with in the viewport transformation instead)
	//Its an off by one error in any event so we may never know
	width = (u8)(x2-x1)+1;
	height = (u8)(y2-y1)+1;
	x = x1;
	y = y1;
}

void gfx3d_glFogColor(u32 v)
{
	gfx3d.state.fogColor = v;
}

void gfx3d_glFogOffset(u32 v)
{
	gfx3d.state.fogOffset = (v & 0x7FFF);
}

void gfx3d_glClearDepth(u32 v)
{
	gfx3d.state.clearDepth = DS_DEPTH15TO24(v);
}

int gfx3d_GetNumPolys()
{
	//so is this in the currently-displayed or currently-built list?
	return (polylists[listTwiddle].count);
}

int gfx3d_GetNumVertex()
{
	//so is this in the currently-displayed or currently-built list?
	return (vertListCount[listTwiddle]);
}

void gfx3d_UpdateToonTable(u8 offset, u16 val)
{
	gfx3d.state.invalidateToon = true;
	gfx3d.state.u16ToonTable[offset] = val;
	//printf("toon %d set to %04X\n",offset,val);
}

void gfx3d_UpdateToonTable(u8 offset, u32 val)
{
	//C.O.P. sets toon table via this method
	gfx3d.state.invalidateToon = true;
	gfx3d.state.u16ToonTable[offset] = val & 0xFFFF;
	gfx3d.state.u16ToonTable[offset+1] = val >> 16;
	//printf("toon %d set to %04X\n",offset,gfx3d.state.u16ToonTable[offset]);
	//printf("toon %d set to %04X\n",offset+1,gfx3d.state.u16ToonTable[offset+1]);
}

s32 gfx3d_GetClipMatrix(const u32 index)
{
	//printf("reading clip matrix: %d\n",index);
	return MatrixGetMultipliedIndex(index, mtxCurrent[MATRIXMODE_PROJECTION], mtxCurrent[MATRIXMODE_POSITION]);
}

s32 gfx3d_GetDirectionalMatrix(const u32 index)
{
	const size_t _index = (((index / 3) * 4) + (index % 3));

	//return (s32)(mtxCurrent[2][_index]*(1<<12));
	return mtxCurrent[MATRIXMODE_POSITION_VECTOR][_index];
}

void gfx3d_glAlphaFunc(u32 v)
{
	gfx3d.state.alphaTestRef = v & 0x1F;
}

u32 gfx3d_glGetPosRes(const size_t index)
{
	return (u32)(s32)(PTcoords[index] * 4096.0f);
}

//#define _3D_LOG_EXEC
#ifdef _3D_LOG_EXEC
static void log3D(u8 cmd, u32 param)
{
	INFO("3D command 0x%02X: ", cmd);
	switch (cmd)
		{
			case 0x10:		// MTX_MODE - Set Matrix Mode (W)
				printf("MTX_MODE(%08X)", param);
			break;
			case 0x11:		// MTX_PUSH - Push Current Matrix on Stack (W)
				printf("MTX_PUSH()\t");
			break;
			case 0x12:		// MTX_POP - Pop Current Matrix from Stack (W)
				printf("MTX_POP(%08X)", param);
			break;
			case 0x13:		// MTX_STORE - Store Current Matrix on Stack (W)
				printf("MTX_STORE(%08X)", param);
			break;
			case 0x14:		// MTX_RESTORE - Restore Current Matrix from Stack (W)
				printf("MTX_RESTORE(%08X)", param);
			break;
			case 0x15:		// MTX_IDENTITY - Load Unit Matrix to Current Matrix (W)
				printf("MTX_IDENTITY()\t");
			break;
			case 0x16:		// MTX_LOAD_4x4 - Load 4x4 Matrix to Current Matrix (W)
				printf("MTX_LOAD_4x4(%08X)", param);
			break;
			case 0x17:		// MTX_LOAD_4x3 - Load 4x3 Matrix to Current Matrix (W)
				printf("MTX_LOAD_4x3(%08X)", param);
			break;
			case 0x18:		// MTX_MULT_4x4 - Multiply Current Matrix by 4x4 Matrix (W)
				printf("MTX_MULT_4x4(%08X)", param);
			break;
			case 0x19:		// MTX_MULT_4x3 - Multiply Current Matrix by 4x3 Matrix (W)
				printf("MTX_MULT_4x3(%08X)", param);
			break;
			case 0x1A:		// MTX_MULT_3x3 - Multiply Current Matrix by 3x3 Matrix (W)
				printf("MTX_MULT_3x3(%08X)", param);
			break;
			case 0x1B:		// MTX_SCALE - Multiply Current Matrix by Scale Matrix (W)
				printf("MTX_SCALE(%08X)", param);
			break;
			case 0x1C:		// MTX_TRANS - Mult. Curr. Matrix by Translation Matrix (W)
				printf("MTX_TRANS(%08X)", param);
			break;
			case 0x20:		// COLOR - Directly Set Vertex Color (W)
				printf("COLOR(%08X)", param);
			break;
			case 0x21:		// NORMAL - Set Normal Vector (W)
				printf("NORMAL(%08X)", param);
			break;
			case 0x22:		// TEXCOORD - Set Texture Coordinates (W)
				printf("TEXCOORD(%08X)", param);
			break;
			case 0x23:		// VTX_16 - Set Vertex XYZ Coordinates (W)
				printf("VTX_16(%08X)", param);
			break;
			case 0x24:		// VTX_10 - Set Vertex XYZ Coordinates (W)
				printf("VTX_10(%08X)", param);
			break;
			case 0x25:		// VTX_XY - Set Vertex XY Coordinates (W)
				printf("VTX_XY(%08X)", param);
			break;
			case 0x26:		// VTX_XZ - Set Vertex XZ Coordinates (W)
				printf("VTX_XZ(%08X)", param);
			break;
			case 0x27:		// VTX_YZ - Set Vertex YZ Coordinates (W)
				printf("VTX_YZ(%08X)", param);
			break;
			case 0x28:		// VTX_DIFF - Set Relative Vertex Coordinates (W)
				printf("VTX_DIFF(%08X)", param);
			break;
			case 0x29:		// POLYGON_ATTR - Set Polygon Attributes (W)
				printf("POLYGON_ATTR(%08X)", param);
			break;
			case 0x2A:		// TEXIMAGE_PARAM - Set Texture Parameters (W)
				printf("TEXIMAGE_PARAM(%08X)", param);
			break;
			case 0x2B:		// PLTT_BASE - Set Texture Palette Base Address (W)
				printf("PLTT_BASE(%08X)", param);
			break;
			case 0x30:		// DIF_AMB - MaterialColor0 - Diffuse/Ambient Reflect. (W)
				printf("DIF_AMB(%08X)", param);
			break;
			case 0x31:		// SPE_EMI - MaterialColor1 - Specular Ref. & Emission (W)
				printf("SPE_EMI(%08X)", param);
			break;
			case 0x32:		// LIGHT_VECTOR - Set Light's Directional Vector (W)
				printf("LIGHT_VECTOR(%08X)", param);
			break;
			case 0x33:		// LIGHT_COLOR - Set Light Color (W)
				printf("LIGHT_COLOR(%08X)", param);
			break;
			case 0x34:		// SHININESS - Specular Reflection Shininess Table (W)
				printf("SHININESS(%08X)", param);
			break;
			case 0x40:		// BEGIN_VTXS - Start of Vertex List (W)
				printf("BEGIN_VTXS(%08X)", param);
			break;
			case 0x41:		// END_VTXS - End of Vertex List (W)
				printf("END_VTXS()\t");
			break;
			case 0x50:		// SWAP_BUFFERS - Swap Rendering Engine Buffer (W)
				printf("SWAP_BUFFERS(%08X)", param);
			break;
			case 0x60:		// VIEWPORT - Set Viewport (W)
				printf("VIEWPORT(%08X)", param);
			break;
			case 0x70:		// BOX_TEST - Test if Cuboid Sits inside View Volume (W)
				printf("BOX_TEST(%08X)", param);
			break;
			case 0x71:		// POS_TEST - Set Position Coordinates for Test (W)
				printf("POS_TEST(%08X)", param);
			break;
			case 0x72:		// VEC_TEST - Set Directional Vector for Test (W)
				printf("VEC_TEST(%08X)", param);
			break;
			default:
				INFO("!!! Unknown(%08X)", param);
			break;
		}
		printf("\t\t(FIFO size %i)\n", gxFIFO.size);
}
#endif

static void gfx3d_execute(u8 cmd, u32 param)
{
#ifdef _3D_LOG_EXEC
	log3D(cmd, param);
#endif

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
			gfx3d_glVertex3_cord<0,1>(param);
		break;
		case 0x26:		// VTX_XZ - Set Vertex XZ Coordinates (W)
			gfx3d_glVertex3_cord<0,2>(param);
		break;
		case 0x27:		// VTX_YZ - Set Vertex YZ Coordinates (W)
			gfx3d_glVertex3_cord<1,2>(param);
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

void gfx3d_execute3D()
{
	u8	cmd = 0;
	u32	param = 0;

	//3d engine is locked up, or something.
	//I dont think this should happen....
	if (isSwapBuffers) return;

	//this is a SPEED HACK
	//fifo is currently emulated more accurately than it probably needs to be.
	//without this batch size the emuloop will escape way too often to run fast.
	static const size_t HACK_FIFO_BATCH_SIZE = 64;

	for (size_t i = 0; i < HACK_FIFO_BATCH_SIZE; i++)
	{
		if (GFX_PIPErecv(&cmd, &param))
		{
			//if (isSwapBuffers) printf("Executing while swapbuffers is pending: %d:%08X\n",cmd,param);

			//since we did anything at all, incur a pipeline motion cost.
			//also, we can't let gxfifo sequencer stall until the fifo is empty.
			//see...
			GFX_DELAY(1); 

			//..these guys will ordinarily set a delay, but multi-param operations won't
			//for the earlier params.
			//printf("%05d:%03d:%12lld: executed 3d: %02X %08X\n",currFrameCounter, nds.VCount, nds_timer , cmd, param);
			gfx3d_execute(cmd, param);

			//this is a COMPATIBILITY HACK.
			//this causes 3d to take virtually no time whatsoever to execute.
			//this was done for marvel nemesis, but a similar family of 
			//hacks for ridiculously fast 3d execution has proven necessary for a number of games.
			//the true answer is probably dma bus blocking.. but lets go ahead and try this and
			//check the compatibility, at the very least it will be nice to know if any games suffer from
			//3d running too fast
			MMU.gfx3dCycles = nds_timer+1;
		} else break;
	}

}

void gfx3d_glFlush(u32 v)
{
	//printf("-------------FLUSH------------- (vcount=%d\n",nds.VCount);
	gfx3d.state.pendingFlushCommand = v;
#if 0
	if (isSwapBuffers)
	{
		INFO("Error: swapBuffers already use\n");
	}
#endif
	
	isSwapBuffers = TRUE;

	GFX_DELAY(1);
}

static bool gfx3d_ysort_compare(int num1, int num2)
{
	const POLY &poly1 = *_clippedPolyUnsortedList[num1].poly;
	const POLY &poly2 = *_clippedPolyUnsortedList[num2].poly;
	
	//this may be verified by checking the game create menus in harvest moon island of happiness
	//also the buttons in the knights in the nightmare frontend depend on this and the perspective division
	if (poly1.maxy != poly2.maxy)
		return (poly1.maxy < poly2.maxy);
	if (poly1.miny != poly2.miny)
		return (poly1.miny < poly2.miny);
	
	//notably, the main shop interface in harvest moon will not have a correct RTN button
	//i think this is due to a math error rounding its position to one pixel too high and it popping behind
	//the bar that it sits on.
	//everything else in all the other menus that I could find looks right..
	
	//make sure we respect the game's ordering in cases of complete ties
	//this makes it a stable sort.
	//this must be a stable sort or else advance wars DOR will flicker in the main map mode
	return (num1 < num2);
}

template <ClipperMode CLIPPERMODE>
void gfx3d_PerformClipping(const VERT *vtxList, const POLYLIST *polyList)
{
	bool isPolyUnclipped = false;
	_clipper->Reset();
	
	for (size_t polyIndex = 0, clipCount = 0; polyIndex < polyList->count; polyIndex++)
	{
		const POLY &poly = polyList->list[polyIndex];
		
		const VERT *clipVerts[4] = {
			&vtxList[poly.vertIndexes[0]],
			&vtxList[poly.vertIndexes[1]],
			&vtxList[poly.vertIndexes[2]],
			(poly.type == POLYGON_TYPE_QUAD) ? &vtxList[poly.vertIndexes[3]] : NULL
		};
		
		isPolyUnclipped = _clipper->ClipPoly<CLIPPERMODE>(polyIndex, poly, clipVerts);
		
		if (CLIPPERMODE == ClipperMode_DetermineClipOnly)
		{
			if (isPolyUnclipped)
			{
				_clippedPolyUnsortedList[polyIndex].index = _clipper->GetClippedPolyByIndex(clipCount).index;
				_clippedPolyUnsortedList[polyIndex].poly = _clipper->GetClippedPolyByIndex(clipCount).poly;
				clipCount++;
			}
		}
		else
		{
			if (isPolyUnclipped)
			{
				_clippedPolyUnsortedList[polyIndex] = _clipper->GetClippedPolyByIndex(clipCount);
				clipCount++;
			}
		}
	}
}

void gfx3d_GenerateRenderLists(const ClipperMode clippingMode)
{
	switch (clippingMode)
	{
		case ClipperMode_Full:
			gfx3d_PerformClipping<ClipperMode_Full>(gfx3d.vertList, gfx3d.polylist);
			break;
			
		case ClipperMode_FullColorInterpolate:
			gfx3d_PerformClipping<ClipperMode_FullColorInterpolate>(gfx3d.vertList, gfx3d.polylist);
			break;
			
		case ClipperMode_DetermineClipOnly:
			gfx3d_PerformClipping<ClipperMode_DetermineClipOnly>(gfx3d.vertList, gfx3d.polylist);
			break;
	}
	
	gfx3d.clippedPolyCount = _clipper->GetPolyCount();

#ifdef _SHOW_VTX_COUNTERS
	max_polys = max((u32)polycount, max_polys);
	max_verts = max((u32)vertListCount[listTwiddle], max_verts);
	osd->addFixed(180, 20, "%i/%i", polycount, vertListCount[listTwiddle]);		// current
	osd->addFixed(180, 35, "%i/%i", max_polys, max_verts);		// max
#endif
	
	//we need to sort the poly list with alpha polys last
	//first, look for opaque polys
	size_t ctr = 0;
	for (size_t i = 0; i < gfx3d.clippedPolyCount; i++)
	{
		const CPoly &clippedPoly = _clipper->GetClippedPolyByIndex(i);
		if (!clippedPoly.poly->isTranslucent())
			gfx3d.indexlist.list[ctr++] = clippedPoly.index;
	}
	gfx3d.clippedPolyOpaqueCount = ctr;
	
	//then look for translucent polys
	for (size_t i = 0; i < gfx3d.clippedPolyCount; i++)
	{
		const CPoly &clippedPoly = _clipper->GetClippedPolyByIndex(i);
		if (clippedPoly.poly->isTranslucent())
			gfx3d.indexlist.list[ctr++] = clippedPoly.index;
	}
	
	//find the min and max y values for each poly.
	//the w-division here is just an approximation to fix the shop in harvest moon island of happiness
	//also the buttons in the knights in the nightmare frontend depend on this
	//we process all polys here instead of just the opaque ones (which have been sorted to the beginning of the index list earlier) because
	//1. otherwise it is basically two passes through the list and will probably run slower than one pass through the list
	//2. most geometry is opaque which is always sorted anyway
	for (size_t i = 0; i < gfx3d.clippedPolyCount; i++)
	{
		// TODO: Possible divide by zero with the w-coordinate.
		// Is the vertex being read correctly? Is 0 a valid value for w?
		// If both of these questions answer to yes, then how does the NDS handle a NaN?
		// For now, simply prevent w from being zero.
		POLY &poly = *_clipper->GetClippedPolyByIndex(i).poly;
		float verty = gfx3d.vertList[poly.vertIndexes[0]].y;
		float vertw = (gfx3d.vertList[poly.vertIndexes[0]].w != 0.0f) ? gfx3d.vertList[poly.vertIndexes[0]].w : 0.00000001f;
		verty = 1.0f-(verty+vertw)/(2*vertw);
		poly.miny = poly.maxy = verty;
		
		for (size_t j = 1; j < poly.type; j++)
		{
			verty = gfx3d.vertList[poly.vertIndexes[j]].y;
			vertw = (gfx3d.vertList[poly.vertIndexes[j]].w != 0.0f) ? gfx3d.vertList[poly.vertIndexes[j]].w : 0.00000001f;
			verty = 1.0f-(verty+vertw)/(2*vertw);
			poly.miny = min(poly.miny, verty);
			poly.maxy = max(poly.maxy, verty);
		}
	}

	//now we have to sort the opaque polys by y-value.
	//(test case: harvest moon island of happiness character creator UI)
	//should this be done after clipping??
	std::sort(gfx3d.indexlist.list, gfx3d.indexlist.list + gfx3d.clippedPolyOpaqueCount, gfx3d_ysort_compare);
	
	if (!gfx3d.state.sortmode)
	{
		//if we are autosorting translucent polys, we need to do this also
		//TODO - this is unverified behavior. need a test case
		std::sort(gfx3d.indexlist.list + gfx3d.clippedPolyOpaqueCount, gfx3d.indexlist.list + gfx3d.clippedPolyCount, gfx3d_ysort_compare);
	}
	
	// Reorder the clipped polygon list to match our sorted index list.
	if (clippingMode == ClipperMode_DetermineClipOnly)
	{
		for (size_t i = 0; i < gfx3d.clippedPolyCount; i++)
		{
			_clippedPolySortedList[i].poly = _clippedPolyUnsortedList[gfx3d.indexlist.list[i]].poly;
		}
	}
	else
	{
		for (size_t i = 0; i < gfx3d.clippedPolyCount; i++)
		{
			_clippedPolySortedList[i] = _clippedPolyUnsortedList[gfx3d.indexlist.list[i]];
		}
	}
}

static void gfx3d_doFlush()
{
	gfx3d.render3DFrameCount++;

	//the renderer will get the lists we just built
	gfx3d.polylist = polylist;
	gfx3d.vertList = vertList;
	gfx3d.vertListCount = vertListCount[listTwiddle];

	//and also our current render state
	gfx3d.state.sortmode = BIT0(gfx3d.state.activeFlushCommand);
	gfx3d.state.wbuffer = BIT1(gfx3d.state.activeFlushCommand);

	//latch the current renderer and geometry engine states
	//NOTE: the geometry lists are copied elsewhere by another operation.
	//that's pretty annoying.
	gfx3d.renderState = gfx3d.state;
	
	gfx3d.state.activeFlushCommand = gfx3d.state.pendingFlushCommand;
	
	const ClipperMode clippingMode = CurrentRenderer->GetPreferredPolygonClippingMode();
	gfx3d_GenerateRenderLists(clippingMode);
	
	//switch to the new lists
	twiddleLists();

	if (driver->view3d->IsRunning())
	{
		viewer3d_state->frameNumber = currFrameCounter;
		viewer3d_state->state = gfx3d.state;
		viewer3d_state->polylist = *gfx3d.polylist;
		viewer3d_state->indexlist = gfx3d.indexlist;
		viewer3d_state->vertListCount = gfx3d.vertListCount;
		
		memcpy(viewer3d_state->vertList, gfx3d.vertList, gfx3d.vertListCount * sizeof(VERT));
		
		driver->view3d->NewFrame();
	}

	drawPending = TRUE;
}

void gfx3d_VBlankSignal()
{
	if (isSwapBuffers)
	{
		gfx3d_doFlush();
		GFX_DELAY(392);
		isSwapBuffers = FALSE;
		
		//let's consider this the beginning of the next 3d frame.
		//in case the game isn't constantly restoring the projection matrix, we want to ping lua
		UpdateProjection();
	}
}

void gfx3d_VBlankEndSignal(bool skipFrame)
{
	if (CurrentRenderer->GetRenderNeedsFinish())
	{
		GPU->ForceRender3DFinishAndFlush(false);
		CurrentRenderer->SetRenderNeedsFinish(false);
		GPU->GetEventHandler()->DidRender3DEnd();
	}

	//try powering on rendering for the next frame.
	//this is a wild guess. it isnt clear what various timings of powering off and on will affect whether a valid frame renders
	//its also part of an old and probably bad guess.
	if(!nds.power_render && nds.power1.gfx3d_render)
		nds.power_render = TRUE;
	else if(nds.power_render && !nds.power1.gfx3d_render)
		nds.power_render = FALSE;
	
	//HACK:
	//if a clear image is enabled, its contents could switch AT ANY TIME. Even if no scene has been re-rendered. (Monster Trucks depends on this)
	//therefore we need to continually rerender.
	//But for that matter, so too could texture data. Do we continually re-render for that?
	//Well, we have no evidence that anyone does this. (seems useful for an FMV, maybe)
	//But also, for that matter, so too could CAPTURED DATA. Hopefully nobody does this.
	//So, for now, we're calling this a hack for the clear image only. 
	//It will increase CPU load for apparently no purpose in some games which switch between heavy 3d and 2d.. and that also use clear images.. that seems unlikely.
	//This could be a candidate for a per-game hack later if it proves unwieldy.
	//Logic to determine whether the contents have changed (due to register changes or vram remapping to LCDC) are going to be really messy, but maybe workable if everything else wasn't so messy.
	//While we're on the subject, a game could set a clear image and never even render anything.
	//The time to fix this will be when we rearchitect things to have more geometry processing lower-level in GFX3D..
	//..since we'll be ripping a lot of stuff apart anyway
	bool forceDraw = false;
	if(gfx3d.state.enableClearImage) {
		forceDraw = true;
		//Well, now this makes it definite HACK caliber
		//We're using this as a sentinel for whether anything's ever been drawn--since we glitch out and crashing trying to render (if only for the clear image)
		//when no geometry's ever been swapped in
		if(!gfx3d.renderState.fogDensityTable)
			forceDraw = false;
	}
	
	if (!drawPending && !forceDraw)
		return;

	if (skipFrame) return;

	drawPending = FALSE;
	
	GPU->GetEventHandler()->DidApplyRender3DSettingsBegin();
	
	const ClipperMode oldClippingMode = CurrentRenderer->GetPreferredPolygonClippingMode();
	GPU->Change3DRendererIfNeeded();
	const ClipperMode newClippingMode = CurrentRenderer->GetPreferredPolygonClippingMode();
	
	if (oldClippingMode != newClippingMode)
	{
		gfx3d_GenerateRenderLists(newClippingMode);
	}
	
	CurrentRenderer->ApplyRenderingSettings(gfx3d.renderState);
	GPU->GetEventHandler()->DidApplyRender3DSettingsEnd();
	
	GPU->GetEventHandler()->DidRender3DBegin();
	CurrentRenderer->SetRenderNeedsFinish(true);
	
	//the timing of powering on rendering may not be exactly right here.
	if (GPU->GetEngineMain()->GetEnableStateApplied() && nds.power_render)
	{
		CurrentRenderer->SetTextureProcessingProperties();
		CurrentRenderer->Render(gfx3d);
	}
	else
	{
		CurrentRenderer->RenderPowerOff();
	}
}

//#define _3D_LOG

void gfx3d_sendCommandToFIFO(u32 val)
{
	//printf("gxFIFO: send val=0x%08X, size=%03i (fifo)\n", val, gxFIFO.size);

	gxf_hardware.receive(val);
}

void gfx3d_sendCommand(u32 cmd, u32 param)
{
	cmd = (cmd & 0x01FF) >> 2;

	//printf("gxFIFO: send 0x%02X: val=0x%08X, size=%03i (direct)\n", cmd, param, gxFIFO.size);

#ifdef _3D_LOG
	INFO("gxFIFO: send 0x%02X: val=0x%08X, pipe %02i, fifo %03i (direct)\n", cmd, param, gxPIPE.tail, gxFIFO.tail);
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
			//printf("mmu: sending %02X: %08X\n", cmd,param);
			GFX_FIFOsend(cmd, param);
			break;
		case 0x50:		// SWAP_BUFFERS - Swap Rendering Engine Buffer (W)
			//printf("mmu: sending %02X: %08X\n", cmd,param);
			GFX_FIFOsend(cmd, param);
		break;
		default:
			INFO("Unknown 3D command %03X with param 0x%08X (directport)\n", cmd, param);
			break;
	}
}

//--------------
//other misc stuff
template<MatrixMode MODE>
void gfx3d_glGetMatrix(const int index, float (&dst)[16])
{
	if (index == -1)
	{
		MatrixCopy(dst, mtxCurrent[MODE]);
	}
	else
	{
		switch (MODE)
		{
			case MATRIXMODE_PROJECTION:
				MatrixCopy(dst, mtxStackProjection.matrix[0]);
				break;
				
			case MATRIXMODE_POSITION:
				MatrixCopy(dst, mtxStackPosition.matrix[0]);
				break;
				
			case MATRIXMODE_POSITION_VECTOR:
				MatrixCopy(dst, mtxStackPositionVector.matrix[0]);
				break;
				
			case MATRIXMODE_TEXTURE:
				MatrixCopy(dst, mtxStackTexture.matrix[0]);
				break;
				
			default:
				break;
		}
	}
}

void gfx3d_glGetLightDirection(const size_t index, u32 &dst)
{
	dst = lightDirection[index];
}

void gfx3d_glGetLightColor(const size_t index, u32 &dst)
{
	dst = lightColor[index];
}

//http://www.opengl.org/documentation/specs/version1.1/glspec1.1/node17.html
//talks about the state required to process verts in quadlists etc. helpful ideas.
//consider building a little state structure that looks exactly like this describes

SFORMAT SF_GFX3D[]={
	{ "GCTL", 4, 1, &gfx3d.state.savedDISP3DCNT},
	{ "GPAT", 4, 1, &polyAttrInProcess.value},
	{ "GPAP", 4, 1, &currentPolyAttr.value},
	{ "GINB", 4, 1, &inBegin},
	{ "GTFM", 4, 1, &currentPolyTexParam.value},
	{ "GTPA", 4, 1, &currentPolyTexPalette},
	{ "GMOD", 4, 1, &mode},
	{ "GMTM", 4,16, mtxTemporal},
	{ "GMCU", 4,64, mtxCurrent},
	{ "ML4I", 1, 1, &ML4x4ind},
	{ "ML3I", 1, 1, &ML4x3ind},
	{ "MM4I", 1, 1, &MM4x4ind},
	{ "MM3I", 1, 1, &MM4x3ind},
	{ "MMxI", 1, 1, &MM3x3ind},
	{ "GSCO", 2, 4, s16coord},
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
	{ "GLSB", 4, 1, &isSwapBuffers},
	{ "GLBT", 4, 1, &BTind},
	{ "GLPT", 4, 1, &PTind},
	{ "GLPC", 4, 4, PTcoords},
	{ "GBTC", 2, 6, &BTcoords[0]},
	{ "GFHE", 4, 1, &gxFIFO.head},
	{ "GFTA", 4, 1, &gxFIFO.tail},
	{ "GFSZ", 4, 1, &gxFIFO.size},
	{ "GFMS", 4, 1, &gxFIFO.matrix_stack_op_size},
	{ "GFCM", 1, HACK_GXIFO_SIZE, &gxFIFO.cmd[0]},
	{ "GFPM", 4, HACK_GXIFO_SIZE, &gxFIFO.param[0]},
	{ "GPHE", 1, 1, &gxPIPE.head},
	{ "GPTA", 1, 1, &gxPIPE.tail},
	{ "GPSZ", 1, 1, &gxPIPE.size},
	{ "GPCM", 1, 4, &gxPIPE.cmd[0]},
	{ "GPPM", 4, 4, &gxPIPE.param[0]},
	{ "GCOL", 1, 4, &colorRGB[0]},
	{ "GLCO", 4, 4, lightColor},
	{ "GLDI", 4, 4, lightDirection},
	{ "GMDI", 2, 1, &dsDiffuse},
	{ "GMAM", 2, 1, &dsAmbient},
	{ "GMSP", 2, 1, &dsSpecular},
	{ "GMEM", 2, 1, &dsEmission},
	{ "GDRP", 4, 1, &drawPending},
	{ "GSET", 4, 1, &gfx3d.state.enableTexturing},
	{ "GSEA", 4, 1, &gfx3d.state.enableAlphaTest},
	{ "GSEB", 4, 1, &gfx3d.state.enableAlphaBlending},
	{ "GSEX", 4, 1, &gfx3d.state.enableAntialiasing},
	{ "GSEE", 4, 1, &gfx3d.state.enableEdgeMarking},
	{ "GSEC", 4, 1, &gfx3d.state.enableClearImage},
	{ "GSEF", 4, 1, &gfx3d.state.enableFog},
	{ "GSEO", 4, 1, &gfx3d.state.enableFogAlphaOnly},
	{ "GFSH", 4, 1, &gfx3d.state.fogShift},
	{ "GSSH", 4, 1, &gfx3d.state.shading},
	{ "GSWB", 4, 1, &gfx3d.state.wbuffer},
	{ "GSSM", 4, 1, &gfx3d.state.sortmode},
	{ "GSAR", 1, 1, &gfx3d.state.alphaTestRef},
	{ "GSCC", 4, 1, &gfx3d.state.clearColor},
	{ "GSCD", 4, 1, &gfx3d.state.clearDepth},
	{ "GSFC", 4, 4, &gfx3d.state.fogColor},
	{ "GSFO", 4, 1, &gfx3d.state.fogOffset},
	{ "GST4", 2, 32, gfx3d.state.u16ToonTable},
	{ "GSSU", 1, 128, &gfx3d.state.shininessTable[0]},
	{ "GSAF", 4, 1, &gfx3d.state.activeFlushCommand},
	{ "GSPF", 4, 1, &gfx3d.state.pendingFlushCommand},

	{ "gSET", 4, 1, &gfx3d.renderState.enableTexturing},
	{ "gSEA", 4, 1, &gfx3d.renderState.enableAlphaTest},
	{ "gSEB", 4, 1, &gfx3d.renderState.enableAlphaBlending},
	{ "gSEX", 4, 1, &gfx3d.renderState.enableAntialiasing},
	{ "gSEE", 4, 1, &gfx3d.renderState.enableEdgeMarking},
	{ "gSEC", 4, 1, &gfx3d.renderState.enableClearImage},
	{ "gSEF", 4, 1, &gfx3d.renderState.enableFog},
	{ "gSEO", 4, 1, &gfx3d.renderState.enableFogAlphaOnly},
	{ "gFSH", 4, 1, &gfx3d.renderState.fogShift},
	{ "gSSH", 4, 1, &gfx3d.renderState.shading},
	{ "gSWB", 4, 1, &gfx3d.renderState.wbuffer},
	{ "gSSM", 4, 1, &gfx3d.renderState.sortmode},
	{ "gSAR", 1, 1, &gfx3d.renderState.alphaTestRef},
	{ "gSCC", 4, 1, &gfx3d.renderState.clearColor},
	{ "gSCD", 4, 1, &gfx3d.renderState.clearDepth},
	{ "gSFC", 4, 4, &gfx3d.renderState.fogColor},
	{ "gSFO", 4, 1, &gfx3d.renderState.fogOffset},
	{ "gST4", 2, 32, gfx3d.renderState.u16ToonTable},
	{ "gSSU", 1, 128, &gfx3d.renderState.shininessTable[0]},
	{ "gSAF", 4, 1, &gfx3d.renderState.activeFlushCommand},
	{ "gSPF", 4, 1, &gfx3d.renderState.pendingFlushCommand},

	{ "GSVP", 4, 1, &viewport},
	{ "GSSI", 1, 1, &shininessInd},
	//------------------------
	{ "GTST", 1, 1, &triStripToggle},
	{ "GTVC", 4, 1, &tempVertInfo.count},
	{ "GTVM", 4, 4, tempVertInfo.map},
	{ "GTVF", 4, 1, &tempVertInfo.first},
	{ "G3CX", 1, 4*GPU_FRAMEBUFFER_NATIVE_WIDTH*GPU_FRAMEBUFFER_NATIVE_HEIGHT, _gfx3d_savestateBuffer},
	{ 0 }
};

//-------------savestate
void gfx3d_PrepareSaveStateBufferWrite()
{
	if (CurrentRenderer->GetRenderNeedsFinish())
	{
		GPU->ForceRender3DFinishAndFlush(true);
	}
	
	const size_t w = CurrentRenderer->GetFramebufferWidth();
	const size_t h = CurrentRenderer->GetFramebufferHeight();
	
	if ( (w == GPU_FRAMEBUFFER_NATIVE_WIDTH) && (h == GPU_FRAMEBUFFER_NATIVE_HEIGHT) ) // Framebuffer is at the native size
	{
		if (CurrentRenderer->GetColorFormat() == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvertBuffer6665To8888<false, false>((u32 *)CurrentRenderer->GetFramebuffer(), (u32 *)_gfx3d_savestateBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		}
		else
		{
			ColorspaceCopyBuffer32<false, false>((u32 *)CurrentRenderer->GetFramebuffer(), (u32 *)_gfx3d_savestateBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		}
	}
	else // Framebuffer is at a custom size
	{
		const FragmentColor *__restrict src = CurrentRenderer->GetFramebuffer();
		FragmentColor *__restrict dst = _gfx3d_savestateBuffer;
		
		for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
		{
			const GPUEngineLineInfo &lineInfo = GPU->GetLineInfoAtIndex(l);
			CopyLineReduceHinted<0xFFFF, false, true, 4>(lineInfo, src, dst);
			src += lineInfo.pixelCount;
			dst += GPU_FRAMEBUFFER_NATIVE_WIDTH;
		}
		
		if (CurrentRenderer->GetColorFormat() == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvertBuffer6665To8888<false, false>((u32 *)_gfx3d_savestateBuffer, (u32 *)_gfx3d_savestateBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		}
	}
}

void gfx3d_savestate(EMUFILE &os)
{
	//version
	os.write_32LE(4);

	//dump the render lists
	os.write_32LE((u32)vertListCount[listTwiddle]);
	for (size_t i = 0; i < vertListCount[listTwiddle]; i++)
		vertList[i].save(os);
	
	os.write_32LE((u32)polylist->count);
	for (size_t i = 0; i < polylist->count; i++)
		polylist->list[i].save(os);

	// Write matrix stack data
	os.write_32LE(mtxStackProjection.position);
	for (size_t j = 0; j < 16; j++)
	{
		os.write_32LE(mtxStackProjection.matrix[0][j]);
	}
	
	os.write_32LE(mtxStackPosition.position);
	for (size_t i = 0; i < MatrixStack<MATRIXMODE_POSITION>::size; i++)
	{
		for (size_t j = 0; j < 16; j++)
		{
			os.write_32LE(mtxStackPosition.matrix[i][j]);
		}
	}
	
	os.write_32LE(mtxStackPositionVector.position);
	for (size_t i = 0; i < MatrixStack<MATRIXMODE_POSITION_VECTOR>::size; i++)
	{
		for (size_t j = 0; j < 16; j++)
		{
			os.write_32LE(mtxStackPositionVector.matrix[i][j]);
		}
	}
	
	os.write_32LE(mtxStackTexture.position);
	for (size_t j = 0; j < 16; j++)
	{
		os.write_32LE(mtxStackTexture.matrix[0][j]);
	}

	gxf_hardware.savestate(os);

	// evidently these need to be saved because we don't cache the matrix that would need to be used to properly regenerate them
	for (size_t i = 0; i < 4; i++)
	{
		for (size_t j = 0; j < 4; j++)
		{
			os.write_32LE(cacheLightDirection[i][j]);
		}
	}
	
	for (size_t i = 0; i < 4; i++)
	{
		for (size_t j = 0; j < 4; j++)
		{
			os.write_32LE(cacheHalfVector[i][j]);
		}
	}
}

bool gfx3d_loadstate(EMUFILE &is, int size)
{
	int version;
	if (is.read_32LE(version) != 1) return false;
	if (size == 8) version = 0;

	if (CurrentRenderer->GetRenderNeedsFinish())
	{
		GPU->ForceRender3DFinishAndFlush(false);
	}

	gfx3d_glPolygonAttrib_cache();
	gfx3d_glTexImage_cache();
	gfx3d_glLightDirection_cache(0);
	gfx3d_glLightDirection_cache(1);
	gfx3d_glLightDirection_cache(2);
	gfx3d_glLightDirection_cache(3);

	//jiggle the lists. and also wipe them. this is clearly not the best thing to be doing.
	listTwiddle = 0;
	polylist = &polylists[0];
	vertList = vertLists;
	
	gfx3d_parseCurrentDISP3DCNT();
	
	if (version >= 1)
	{
		u32 vertListCount32 = 0;
		u32 polyListCount32 = 0;
		
		is.read_32LE(vertListCount32);
		vertListCount[0] = vertListCount32;
		for (size_t i = 0; i < vertListCount[0]; i++)
			vertList[i].load(is);
		
		is.read_32LE(polyListCount32);
		polylist->count = polyListCount32;
		for (size_t i = 0; i < polylist->count; i++)
			polylist->list[i].load(is);
	}

	if (version >= 2)
	{
		// Read matrix stack data
		is.read_32LE(mtxStackProjection.position);
		for (size_t j = 0; j < 16; j++)
		{
			is.read_32LE(mtxStackProjection.matrix[0][j]);
		}
		
		is.read_32LE(mtxStackPosition.position);
		for (size_t i = 0; i < MatrixStack<MATRIXMODE_POSITION>::size; i++)
		{
			for (size_t j = 0; j < 16; j++)
			{
				is.read_32LE(mtxStackPosition.matrix[i][j]);
			}
		}
		
		is.read_32LE(mtxStackPositionVector.position);
		for (size_t i = 0; i < MatrixStack<MATRIXMODE_POSITION_VECTOR>::size; i++)
		{
			for (size_t j = 0; j < 16; j++)
			{
				is.read_32LE(mtxStackPositionVector.matrix[i][j]);
			}
		}
		
		is.read_32LE(mtxStackTexture.position);
		for (size_t j = 0; j < 16; j++)
		{
			is.read_32LE(mtxStackTexture.matrix[0][j]);
		}
	}

	if (version >= 3)
	{
		gxf_hardware.loadstate(is);
	}

	gfx3d.polylist = &polylists[listTwiddle^1];
	gfx3d.vertList = vertLists + VERTLIST_SIZE;
	gfx3d.polylist->count = 0;
	gfx3d.vertListCount = 0;

	if (version >= 4)
	{
		for (size_t i = 0; i < 4; i++)
		{
			for (size_t j = 0; j < 4; j++)
			{
				is.read_32LE(cacheLightDirection[i][j]);
			}
		}
		
		for (size_t i = 0; i < 4; i++)
		{
			for (size_t j = 0; j < 4; j++)
			{
				is.read_32LE(cacheHalfVector[i][j]);
			}
		}
	}

	return true;
}

void gfx3d_FinishLoadStateBufferRead()
{
	const Render3DDeviceInfo &deviceInfo = CurrentRenderer->GetDeviceInfo();
	
	switch (deviceInfo.renderID)
	{
		case RENDERID_NULL:
			memset(CurrentRenderer->GetFramebuffer(), 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(FragmentColor));
			break;
			
		case RENDERID_SOFTRASTERIZER:
		{
			const size_t w = CurrentRenderer->GetFramebufferWidth();
			const size_t h = CurrentRenderer->GetFramebufferHeight();
			
			if ( (w == GPU_FRAMEBUFFER_NATIVE_WIDTH) && (h == GPU_FRAMEBUFFER_NATIVE_HEIGHT) ) // Framebuffer is at the native size
			{
				if (CurrentRenderer->GetColorFormat() == NDSColorFormat_BGR666_Rev)
				{
					ColorspaceConvertBuffer8888To6665<false, false>((u32 *)_gfx3d_savestateBuffer, (u32 *)CurrentRenderer->GetFramebuffer(), GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				}
				else
				{
					ColorspaceCopyBuffer32<false, false>((u32 *)_gfx3d_savestateBuffer, (u32 *)CurrentRenderer->GetFramebuffer(), GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				}
			}
			else // Framebuffer is at a custom size
			{
				if (CurrentRenderer->GetColorFormat() == NDSColorFormat_BGR666_Rev)
				{
					ColorspaceConvertBuffer8888To6665<false, false>((u32 *)_gfx3d_savestateBuffer, (u32 *)_gfx3d_savestateBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				}
				
				const FragmentColor *__restrict src = _gfx3d_savestateBuffer;
				FragmentColor *__restrict dst = CurrentRenderer->GetFramebuffer();
				
				for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
				{
					const GPUEngineLineInfo &lineInfo = GPU->GetLineInfoAtIndex(l);
					CopyLineExpandHinted<0xFFFF, true, false, true, 4>(lineInfo, src, dst);
					src += GPU_FRAMEBUFFER_NATIVE_WIDTH;
					dst += lineInfo.pixelCount;
				}
			}
			break;
		}
			
		default:
			// Do nothing. Loading the 3D framebuffer is unsupported on this 3D renderer.
			break;
	}
}

void gfx3d_parseCurrentDISP3DCNT()
{
	const IOREG_DISP3DCNT &DISP3DCNT = gfx3d.state.savedDISP3DCNT;
	
	gfx3d.state.enableTexturing		= (DISP3DCNT.EnableTexMapping != 0);
	gfx3d.state.shading				=  DISP3DCNT.PolygonShading;
	gfx3d.state.enableAlphaTest		= (DISP3DCNT.EnableAlphaTest != 0);
	gfx3d.state.enableAlphaBlending	= (DISP3DCNT.EnableAlphaBlending != 0);
	gfx3d.state.enableAntialiasing	= (DISP3DCNT.EnableAntiAliasing != 0);
	gfx3d.state.enableEdgeMarking	= (DISP3DCNT.EnableEdgeMarking != 0);
	gfx3d.state.enableFogAlphaOnly	= (DISP3DCNT.FogOnlyAlpha != 0);
	gfx3d.state.enableFog			= (DISP3DCNT.EnableFog != 0);
	gfx3d.state.fogShift			=  DISP3DCNT.FogShiftSHR;
	gfx3d.state.enableClearImage	= (DISP3DCNT.RearPlaneMode != 0);
}

void ParseReg_DISP3DCNT()
{
	const IOREG_DISP3DCNT &DISP3DCNT = GPU->GetEngineMain()->GetIORegisterMap().DISP3DCNT;
	
	if (gfx3d.state.savedDISP3DCNT.value == DISP3DCNT.value)
	{
		return;
	}
	
	gfx3d.state.savedDISP3DCNT.value = DISP3DCNT.value;
	gfx3d_parseCurrentDISP3DCNT();
}

template void gfx3d_glGetMatrix<MATRIXMODE_PROJECTION>(const int index, float(&dst)[16]);
template void gfx3d_glGetMatrix<MATRIXMODE_POSITION>(const int index, float(&dst)[16]);
template void gfx3d_glGetMatrix<MATRIXMODE_POSITION_VECTOR>(const int index, float(&dst)[16]);
template void gfx3d_glGetMatrix<MATRIXMODE_TEXTURE>(const int index, float(&dst)[16]);

//-------------------
//clipping
//-------------------

// this is optional for now in case someone has trouble with the new method
// or wants to toggle it to compare
#define OPTIMIZED_CLIPPING_METHOD

//#define CLIPLOG(X) printf(X);
//#define CLIPLOG2(X,Y,Z) printf(X,Y,Z);
#define CLIPLOG(X)
#define CLIPLOG2(X,Y,Z)

template<typename T>
static T interpolate(const float ratio, const T& x0, const T& x1)
{
	return (T)(x0 + (float)(x1-x0) * (ratio));
}

//http://www.cs.berkeley.edu/~ug/slide/pipeline/assignments/as6/discussion.shtml
template <ClipperMode CLIPPERMODE, int COORD, int WHICH>
static FORCEINLINE VERT clipPoint(const VERT *inside, const VERT *outside)
{
	VERT ret;
	const float coord_inside = inside->coord[COORD];
	const float coord_outside = outside->coord[COORD];
	const float w_inside = (WHICH == -1) ? -inside->coord[3] : inside->coord[3];
	const float w_outside = (WHICH == -1) ? -outside->coord[3] : outside->coord[3];
	const float t = (coord_inside - w_inside) / ((w_outside-w_inside) - (coord_outside-coord_inside));
	
#define INTERP(X) ret . X = interpolate(t, inside-> X ,outside-> X )
	
	INTERP(coord[0]); INTERP(coord[1]); INTERP(coord[2]); INTERP(coord[3]);
	
	switch (CLIPPERMODE)
	{
		case ClipperMode_Full:
			INTERP(texcoord[0]); INTERP(texcoord[1]);
			INTERP(color[0]); INTERP(color[1]); INTERP(color[2]);
			ret.color_to_float();
			break;
			
		case ClipperMode_FullColorInterpolate:
			INTERP(texcoord[0]); INTERP(texcoord[1]);
			INTERP(fcolor[0]); INTERP(fcolor[1]); INTERP(fcolor[2]);
			break;
			
		case ClipperMode_DetermineClipOnly:
			// Do nothing.
			break;
	}

	//this seems like a prudent measure to make sure that math doesnt make a point pop back out
	//of the clip volume through interpolation
	if (WHICH == -1)
		ret.coord[COORD] = -ret.coord[3];
	else
		ret.coord[COORD] = ret.coord[3];

	return ret;
}

#define MAX_SCRATCH_CLIP_VERTS (4*6 + 40)
static VERT scratchClipVerts[MAX_SCRATCH_CLIP_VERTS];
static size_t numScratchClipVerts = 0;

template <ClipperMode CLIPPERMODE, int COORD, int WHICH, class NEXT>
class ClipperPlane
{
public:
	ClipperPlane(NEXT& next) : m_next(next) {}

	void init(VERT* verts)
	{
		m_prevVert =  NULL;
		m_firstVert = NULL;
		m_next.init(verts);
	}

	void clipVert(const VERT *vert)
	{
		if (m_prevVert)
			this->clipSegmentVsPlane(m_prevVert, vert);
		else
			m_firstVert = (VERT *)vert;
		m_prevVert = (VERT *)vert;
	}

	// closes the loop and returns the number of clipped output verts
	int finish()
	{
		this->clipVert(m_firstVert);
		return m_next.finish();
	}

private:
	VERT* m_prevVert;
	VERT* m_firstVert;
	NEXT& m_next;
	
	FORCEINLINE void clipSegmentVsPlane(const VERT *vert0, const VERT *vert1)
	{
		const float *vert0coord = vert0->coord;
		const float *vert1coord = vert1->coord;
		const bool out0 = (WHICH == -1) ? (vert0coord[COORD] < -vert0coord[3]) : (vert0coord[COORD] > vert0coord[3]);
		const bool out1 = (WHICH == -1) ? (vert1coord[COORD] < -vert1coord[3]) : (vert1coord[COORD] > vert1coord[3]);
		
		//CONSIDER: should we try and clip things behind the eye? does this code even successfully do it? not sure.
		//if(coord==2 && which==1) {
		//	out0 = vert0coord[2] < 0;
		//	out1 = vert1coord[2] < 0;
		//}

		//both outside: insert no points
		if (out0 && out1)
		{
			CLIPLOG(" both outside\n");
		}

		//both inside: insert the next point
		if (!out0 && !out1)
		{
			CLIPLOG(" both inside\n");
			m_next.clipVert(vert1);
		}

		//exiting volume: insert the clipped point
		if (!out0 && out1)
		{
			CLIPLOG(" exiting\n");
			assert((u32)numScratchClipVerts < MAX_SCRATCH_CLIP_VERTS);
			scratchClipVerts[numScratchClipVerts] = clipPoint<CLIPPERMODE, COORD, WHICH>(vert0, vert1);
			m_next.clipVert(&scratchClipVerts[numScratchClipVerts++]);
		}

		//entering volume: insert clipped point and the next (interior) point
		if (out0 && !out1)
		{
			CLIPLOG(" entering\n");
			assert((u32)numScratchClipVerts < MAX_SCRATCH_CLIP_VERTS);
			scratchClipVerts[numScratchClipVerts] = clipPoint<CLIPPERMODE, COORD, WHICH>(vert1, vert0);
			m_next.clipVert(&scratchClipVerts[numScratchClipVerts++]);
			m_next.clipVert(vert1);
		}
	}
};

class ClipperOutput
{
public:
	void init(VERT* verts)
	{
		m_nextDestVert = verts;
		m_numVerts = 0;
	}
	
	void clipVert(const VERT *vert)
	{
		assert((u32)m_numVerts < MAX_CLIPPED_VERTS);
		*m_nextDestVert++ = *vert;
		m_numVerts++;
	}
	
	int finish()
	{
		return m_numVerts;
	}
	
private:
	VERT* m_nextDestVert;
	int m_numVerts;
};

// see "Template juggling with Sutherland-Hodgman" http://www.codeguru.com/cpp/misc/misc/graphics/article.php/c8965__2/
// for the idea behind setting things up like this.

// Non-interpolated clippers
static ClipperOutput clipperOut;
typedef ClipperPlane<ClipperMode_Full, 2, 1,ClipperOutput> Stage6; static Stage6 clipper6 (clipperOut); // back plane //TODO - we need to parameterize back plane clipping
typedef ClipperPlane<ClipperMode_Full, 2,-1,Stage6> Stage5;        static Stage5 clipper5 (clipper6); // front plane
typedef ClipperPlane<ClipperMode_Full, 1, 1,Stage5> Stage4;        static Stage4 clipper4 (clipper5); // top plane
typedef ClipperPlane<ClipperMode_Full, 1,-1,Stage4> Stage3;        static Stage3 clipper3 (clipper4); // bottom plane
typedef ClipperPlane<ClipperMode_Full, 0, 1,Stage3> Stage2;        static Stage2 clipper2 (clipper3); // right plane
typedef ClipperPlane<ClipperMode_Full, 0,-1,Stage2> Stage1;        static Stage1 clipper1 (clipper2); // left plane

// Interpolated clippers
static ClipperOutput clipperOuti;
typedef ClipperPlane<ClipperMode_FullColorInterpolate, 2, 1,ClipperOutput> Stage6i; static Stage6i clipper6i (clipperOuti); // back plane //TODO - we need to parameterize back plane clipping
typedef ClipperPlane<ClipperMode_FullColorInterpolate, 2,-1,Stage6i> Stage5i;       static Stage5i clipper5i (clipper6i); // front plane
typedef ClipperPlane<ClipperMode_FullColorInterpolate, 1, 1,Stage5i> Stage4i;       static Stage4i clipper4i (clipper5i); // top plane
typedef ClipperPlane<ClipperMode_FullColorInterpolate, 1,-1,Stage4i> Stage3i;       static Stage3i clipper3i (clipper4i); // bottom plane
typedef ClipperPlane<ClipperMode_FullColorInterpolate, 0, 1,Stage3i> Stage2i;       static Stage2i clipper2i (clipper3i); // right plane
typedef ClipperPlane<ClipperMode_FullColorInterpolate, 0,-1,Stage2i> Stage1i;       static Stage1i clipper1i (clipper2i); // left plane

// Determine's clip status only
static ClipperOutput clipperOutd;
typedef ClipperPlane<ClipperMode_DetermineClipOnly, 2, 1,ClipperOutput> Stage6d; static Stage6d clipper6d (clipperOutd); // back plane //TODO - we need to parameterize back plane clipping
typedef ClipperPlane<ClipperMode_DetermineClipOnly, 2,-1,Stage6d> Stage5d;       static Stage5d clipper5d (clipper6d); // front plane
typedef ClipperPlane<ClipperMode_DetermineClipOnly, 1, 1,Stage5d> Stage4d;       static Stage4d clipper4d (clipper5d); // top plane
typedef ClipperPlane<ClipperMode_DetermineClipOnly, 1,-1,Stage4d> Stage3d;       static Stage3d clipper3d (clipper4d); // bottom plane
typedef ClipperPlane<ClipperMode_DetermineClipOnly, 0, 1,Stage3d> Stage2d;       static Stage2d clipper2d (clipper3d); // right plane
typedef ClipperPlane<ClipperMode_DetermineClipOnly, 0,-1,Stage2d> Stage1d;       static Stage1d clipper1d (clipper2d); // left plane

GFX3D_Clipper::GFX3D_Clipper()
{
	_clippedPolyList = NULL;
	_clippedPolyCounter = 0;
}

const CPoly* GFX3D_Clipper::GetClippedPolyBufferPtr()
{
	return this->_clippedPolyList;
}

void GFX3D_Clipper::SetClippedPolyBufferPtr(CPoly *bufferPtr)
{
	this->_clippedPolyList = bufferPtr;
}

const CPoly& GFX3D_Clipper::GetClippedPolyByIndex(size_t index) const
{
	return this->_clippedPolyList[index];
}

size_t GFX3D_Clipper::GetPolyCount() const
{
	return this->_clippedPolyCounter;
}

void GFX3D_Clipper::Reset()
{
	this->_clippedPolyCounter = 0;
}

template <ClipperMode CLIPPERMODE>
bool GFX3D_Clipper::ClipPoly(const u16 polyIndex, const POLY &poly, const VERT **verts)
{
	CLIPLOG("==Begin poly==\n");

	PolygonType outType;
	const PolygonType type = poly.type;
	numScratchClipVerts = 0;
	
	switch (CLIPPERMODE)
	{
		case ClipperMode_Full:
		{
			clipper1.init(this->_clippedPolyList[this->_clippedPolyCounter].clipVerts);
			for (size_t i = 0; i < type; i++)
				clipper1.clipVert(verts[i]);
			
			outType = (PolygonType)clipper1.finish();
			break;
		}
			
		case ClipperMode_FullColorInterpolate:
		{
			clipper1i.init(this->_clippedPolyList[this->_clippedPolyCounter].clipVerts);
			for (size_t i = 0; i < type; i++)
				clipper1i.clipVert(verts[i]);
			
			outType = (PolygonType)clipper1i.finish();
			break;
		}
			
		case ClipperMode_DetermineClipOnly:
		{
			clipper1d.init(this->_clippedPolyList[this->_clippedPolyCounter].clipVerts);
			for (size_t i = 0; i < type; i++)
				clipper1d.clipVert(verts[i]);
			
			outType = (PolygonType)clipper1d.finish();
			break;
		}
	}
	
	assert((u32)outType < MAX_CLIPPED_VERTS);
	if (outType < POLYGON_TYPE_TRIANGLE)
	{
		//a totally clipped poly. discard it.
		//or, a degenerate poly. we're not handling these right now
		return false;
	}
	else
	{
		CPoly &thePoly = this->_clippedPolyList[this->_clippedPolyCounter];
		thePoly.index = polyIndex;
		thePoly.type = outType;
		thePoly.poly = (POLY *)&poly;
		
		this->_clippedPolyCounter++;
	}
	
	return true;
}
