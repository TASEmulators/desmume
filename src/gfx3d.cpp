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

//#define FLUSHMODE_HACK

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
#include <queue>
//#include "movie.h"

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

#define TESTS_ENABLED 1

#define INVALID_COMMAND 0xFF
#define UNDEFINED_COMMAND 0xCC
static const u8 gfx3d_commandTypes[] = {
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, //invalid commands; no parameters
	0x01,0x00,0x01,0x01,0x01,0x00,0x10,0x0C, 0x10,0x0C,0x09,0x03,0x03,0xCC,0xCC,0xCC, //matrix commands
	0x01,0x01,0x01,0x02,0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01,0xCC,0xCC,0xCC,0xCC, //vertex and per-vertex material commands
	0x01,0x01,0x01,0x01,0x20,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, //lighting engine material commands
	0x01,0x00,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, //begin and end
	0x01,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, //swapbuffers
	0x01,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, //viewport
	0x03,0x02,0x01,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, //tests
	//0x80:
	0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
	0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
	0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
	0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
	0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
	0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
	0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
	0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC, 0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC
};


//todo: it would be nice to make some kind of state machine that more closely models the hardware
//but this is servicable for now
class GXF_Hardware
{
public:
	GXF_Hardware()
	{
		reset();
	}
	void reset() {
		countdown = 0;
		commandCursor = 4;
		for(int i=0;i<4;i++) {
			commandsPending[i].command = 0;
			commandsPending[i].countdown = 0;
		}
	}
	//todo - things in here other than the very first thing involving GFX3D_NOP_NOARG_HACK I am not too sure about.
	void receive(u32 val) {
		bool hack = false;
		if(size()>0 && val != 0 && 
			(front().command == 0x15 || front().command == 0x11 || front().command == 0x41
			|| front().command == GFX3D_NOP_NOARG_HACK  //nintendogs dalmatian sends these I think.
			))
		{
			//apparently a zero is swallowed in this case but if another value is sent 
			//processing will continue
			if(front().command == GFX3D_NOP_NOARG_HACK) { }
			else
			{
				//printf("gxf: sending hack %02X: (dummy=0)\n", front().command);
				GFX_FIFOsend(front().command,0);
			}
			hack = true;
			goto hackTrigger;
		}
		if(countdown>0) {
			//received a parameter
			//printf("gxf: sending %02X: %08X\n", front().command,val);
			//if(commandsPending.front() == GFX3D_NOP_NOARG_HACK)
			//{}
			//else 
				GFX_FIFOsend(front().command,val);
			hackTrigger:
			countdown--;
			while(countdown==0) {
				dequeue();
			trigger:
				//dont set hack to false if you jumped from below! it needs to be true for when you jump down from above.
				//oh my what a mess.
				if(size()==0) break;
				countdown = front().countdown;
				if(!countdown) {
					if(front().command != INVALID_COMMAND 
						&& front().command != GFX3D_NOP_NOARG_HACK //g.i. joe sends these in the form of 0xFF commands 
						){
						//printf("[%06d]gxf: sending %02X: (dummy=0)\n", currFrameCounter,front().command);
						GFX_FIFOsend(front().command,0);
					}
				}
			}
			if(hack) goto decode;
		} else {
			//decode a packed command
decode:
			//printf("[%05d] gxf: decoding %08X\n",currFrameCounter,val);
			
			bool valid = true;
			bool gimpyCommand = false;
			const u8 commands[] = { val&0xFF, (val>>8)&0xFF, (val>>16)&0xFF, (val>>24)&0xFF };
			const u8 commandTypes[] = { gfx3d_commandTypes[commands[0]], gfx3d_commandTypes[commands[1]],gfx3d_commandTypes[commands[2]], gfx3d_commandTypes[commands[3]] };

			for(int i=0;i<4;i++) {
				u8 cmd = commands[i];
				u8 type = commandTypes[i];
				if(type == INVALID_COMMAND) {
					commandsPending[i].command = INVALID_COMMAND;
				} else {
					if(type == UNDEFINED_COMMAND) 
					{
						commandsPending[i].command = GFX3D_NOP_NOARG_HACK;  //enqueue a single undefined command we know how to handle
					}
					else commandsPending[i].command = cmd;
				}
				if(type == UNDEFINED_COMMAND
					//|| type == 0x00 //DON'T DO THIS: galactik football will break if you do (ingame character portraits etc.) as well as nintendogs dalmatian
					) {
					//these are valid commands with no parameters. they might need special handling
					//as long as there is a subsequent defined command with parameters, we're safe
					bool safe = false;
					for(int j=i+1;j<4;j++) {
						if(commandTypes[j] != INVALID_COMMAND) {
							safe = true;
							break;
						}
					}
					if(safe) {
						commandsPending[i].countdown = 0;
					} else {
						//we need to receive a dummy parameter in this case
						commandsPending[i].countdown = 1;
					}
				} else if(type != INVALID_COMMAND) {
					commandsPending[i].countdown = type;
				} else commandsPending[i].countdown = 0;
			}

			commandCursor = 0;
			countdown = front().countdown;
			if(countdown==0) 
				goto trigger;
		}
	}

	struct CommandItem {
		u8 command, countdown;
	} commandsPending[4];

	u32 commandCursor;
	u8 countdown;

private:
	void dequeue() { commandCursor++; }
	CommandItem& front() { return commandsPending[commandCursor]; }
	u32 size() { return 4-commandCursor; }
public:

	void savestate(EMUFILE *f)
	{
		//TODO - next time we invalidate savestates, simplify this format.
		write32le(0,f); //version
		write32le(size(),f);
		for(u32 i=commandCursor;i<4;i++) write8le(commandsPending[i].command,f);
		write32le(0,f);
		for(u32 i=commandCursor;i<4;i++) write8le(commandsPending[i].countdown,f);
		write8le(countdown,f);
	}
	
	bool loadstate(EMUFILE *f)
	{
		u32 version;
		if(read32le(&version,f) != 1) return false;
		if(version != 0) return false;

		u32 tempsize;
		read32le(&tempsize,f);
		commandCursor = 4-tempsize;
		for(u32 i=0;i<commandCursor;i++) commandsPending[i].command = 0;
		for(u32 i=commandCursor;i<4;i++) read8le(&commandsPending[i].command,f);
		read32le(&tempsize,f);
		for(u32 i=0;i<commandCursor;i++) commandsPending[i].countdown = 0;
		for(u32 i=commandCursor;i<4;i++) read8le(&commandsPending[i].countdown,f);

		read8le(&countdown,f);

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
static GFX3D_Clipper boxtestClipper;

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

//TODO - generate this in the static init method more accurately
CACHE_ALIGN const u8 material_3bit_to_6bit[] = {
	0, 8, 16, 26, 34, 44, 52, 63
};

//private acceleration tables
static float float16table[65536];
static float float10Table[1024];
static float float10RelTable[1024];
static float normalTable[1024];

#define fix2float(v)    (((float)((s32)(v))) / (float)(1<<12))
#define fix10_2float(v) (((float)((s32)(v))) / (float)(1<<9))

CACHE_ALIGN u8 gfx3d_convertedScreen[256*192*4];

// Matrix stack handling
CACHE_ALIGN MatrixStack	mtxStack[4] = {
	MatrixStack(1), // Projection stack
	MatrixStack(31), // Coordinate stack
	MatrixStack(31), // Directional stack
	MatrixStack(1), // Texture stack
};

int _hack_getMatrixStackLevel(int which) { return mtxStack[which].position; }

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
static CACHE_ALIGN u16		u16coord[4] = {0, 0, 0, 0};
static char		coordind = 0;
static u32 vtxFormat = 0;
static BOOL inBegin = FALSE;

// Data for basic transforms
static CACHE_ALIGN float	trans[4] = {0.0, 0.0, 0.0, 0.0};
static int		transind = 0;
static CACHE_ALIGN float	scale[4] = {0.0, 0.0, 0.0, 0.0};
static int		scaleind = 0;
static u32 viewport = 0;

//various other registers
static float _t=0, _s=0;
static float last_t, last_s;
static u32 clCmd = 0;
static u32 clInd = 0;

static u32 clInd2 = 0;
BOOL isSwapBuffers = FALSE;

static u32 BTind = 0;
static u32 PTind = 0;
static u16 BTcoords[6] = {0, 0, 0, 0, 0, 0};
static CACHE_ALIGN float PTcoords[4] = {0.0, 0.0, 0.0, 1.0};

//raw ds format poly attributes
static u32 polyAttr=0,textureFormat=0, texturePalette=0, polyAttrPending=0;

//the current vertex color, 5bit values
static u8 colorRGB[4] = { 31,31,31,31 };

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
int			polygonListCompleted = 0;

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

	if(polylists == NULL) { polylists = new POLYLIST[2]; polylist = &polylists[0]; }
	if(vertlists == NULL) { vertlists = new VERTLIST[2]; vertlist = &vertlists[0]; }
	makeTables();
	gfx3d_reset();
}

void gfx3d_reset()
{
	gfx3d = GFX3D();

	gxf_hardware.reset();

	control = 0;
	drawPending = FALSE;
	flushPending = FALSE;
	memset(polylists, 0, sizeof(polylists));
	memset(vertlists, 0, sizeof(vertlists));
	listTwiddle = 1;
	twiddleLists();
	gfx3d.polylist = polylist;
	gfx3d.vertlist = vertlist;

	polyAttr = 0;
	textureFormat = 0;
	texturePalette = 0;
	polyAttrPending = 0;
	mode = 0;
	u16coord[0] = u16coord[1] = u16coord[2] = u16coord[3] = 0;
	coordind = 0;
	vtxFormat = 0;
	memset(trans, 0, sizeof(trans));
	transind = 0;
	memset(scale, 0, sizeof(scale));
	scaleind = 0;
	viewport = 0;
	memset(gxPIPE.cmd, 0, sizeof(gxPIPE.cmd));
	memset(gxPIPE.param, 0, sizeof(gxPIPE.param));
	memset(colorRGB, 0, sizeof(colorRGB));
	memset(&tempVertInfo, 0, sizeof(tempVertInfo));

	MatrixInit (mtxCurrent[0]);
	MatrixInit (mtxCurrent[1]);
	MatrixInit (mtxCurrent[2]);
	MatrixInit (mtxCurrent[3]);
	MatrixInit (mtxTemporal);

	MatrixStackInit(&mtxStack[0]);
	MatrixStackInit(&mtxStack[1]);
	MatrixStackInit(&mtxStack[2]);
	MatrixStackInit(&mtxStack[3]);

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

	memset(gfx3d_convertedScreen,0,sizeof(gfx3d_convertedScreen));

	gfx3d.clearDepth = gfx3d_extendDepth_15_to_24(0x7FFF);
	
	clInd2 = 0;
	isSwapBuffers = FALSE;

	GFX_PIPEclear();
	GFX_FIFOclear();
}


//================================================================================= Geometry Engine
//=================================================================================
//=================================================================================

#define vec3dot(a, b)		(((a[0]) * (b[0])) + ((a[1]) * (b[1])) + ((a[2]) * (b[2])))
#define SUBMITVERTEX(ii, nn) polylist->list[polylist->count].vertIndexes[ii] = tempVertInfo.map[nn];
//Submit a vertex to the GE
static void SetVertex()
{
	float coord[3] = {
			float16table[u16coord[0]],
			float16table[u16coord[1]],
			float16table[u16coord[2]]
	};

	ALIGN(16) float coordTransformed[4] = { coord[0], coord[1], coord[2], 1.f };

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
	polygonListCompleted = 0;
	if(vertlist->count >= VERTLIST_SIZE) 
			return;
	if(polylist->count >= POLYLIST_SIZE) 
			return;
	
	//TODO - think about keeping the clip matrix concatenated,
	//so that we only have to multiply one matrix here
	//(we could lazy cache the concatenated clip matrix and only generate it
	//when we need to)
	MatrixMultVec4x4_M2(mtxCurrent[0], coordTransformed);

	//TODO - culling should be done here.
	//TODO - viewport transform?

	int continuation = 0;
	if(vtxFormat==2 && !tempVertInfo.first)
		continuation = 2;
	else if(vtxFormat==3 && !tempVertInfo.first)
		continuation = 2;

	//record the vertex
	//VERT &vert = tempVertList.list[tempVertList.count];
	int vertIndex = vertlist->count + tempVertInfo.count - continuation;
	if(vertIndex<0) {
		printf("wtf\n");
	}
	VERT &vert = vertlist->list[vertIndex];

	//printf("%f %f %f\n",coordTransformed[0],coordTransformed[1],coordTransformed[2]);
	//if(coordTransformed[1] > 20) 
	//	coordTransformed[1] = 20;

	//printf("y-> %f\n",coord[1]);

	//if(mtxCurrent[1][14]>15) {
	//	printf("ACK!\n");
	//	printf("----> modelview 1 state for that ack:\n");
	//	//MatrixPrint(mtxCurrent[1]);
	//}

	vert.texcoord[0] = last_s;
	vert.texcoord[1] = last_t;
	vert.coord[0] = coordTransformed[0];
	vert.coord[1] = coordTransformed[1];
	vert.coord[2] = coordTransformed[2];
	vert.coord[3] = coordTransformed[3];
	vert.color[0] = GFX3D_5TO6(colorRGB[0]);
	vert.color[1] = GFX3D_5TO6(colorRGB[1]);
	vert.color[2] = GFX3D_5TO6(colorRGB[2]);
	tempVertInfo.map[tempVertInfo.count] = vertlist->count + tempVertInfo.count - continuation;
	tempVertInfo.count++;

	//possibly complete a polygon
	{
		polygonListCompleted = 2;
		switch(vtxFormat) {
			case 0: //GL_TRIANGLES
				if(tempVertInfo.count!=3)
					break;
				polygonListCompleted = 1;
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
				polygonListCompleted = 1;
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
				polygonListCompleted = 1;
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
				polygonListCompleted = 1;
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
			default: 
				return;
		}

		if(polygonListCompleted == 1)
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

static void gfx3d_glPolygonAttrib_cache()
{
	// Light enable/disable
	lightMask = (polyAttr&0xF);

	// texture environment
	envMode = (polyAttr&0x30)>>4;

	// back face culling
	cullingMask = (polyAttr>>6)&3;
}

static void gfx3d_glTexImage_cache()
{
	texCoordinateTransform = (textureFormat>>30);
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

//===============================================================================
static void gfx3d_glMatrixMode(u32 v)
{
	mode = (v&3);

	GFX_DELAY(1);
}

static void gfx3d_glPushMatrix()
{
	//u32 gxstat = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600);
	//this command always works on both pos and vector when either pos or pos-vector are the current mtx mode
	short mymode = (mode==1?2:mode);

	if (mtxStack[mymode].position > mtxStack[mymode].size)
	{
		MMU_new.gxstat.se = 1;
		//gxstat |= (1<<15);
		return;
	}

	//gxstat &= 0xFFFF00FF;

	MatrixStackPushMatrix(&mtxStack[mymode], mtxCurrent[mymode]);

	GFX_DELAY(17);

	if(mymode==2)
		MatrixStackPushMatrix (&mtxStack[1], mtxCurrent[1]);

	//gxstat |= ((mtxStack[0].position << 13) | (mtxStack[1].position << 8));
}

static void gfx3d_glPopMatrix(s32 i)
{
	//this command always works on both pos and vector when either pos or pos-vector are the current mtx mode
	short mymode = (mode==1?2:mode);

	//6 bits, sign extended
	//this was necessary to fix sims apartment pets
	i = (i<<26)>>26;
	
	if (i > mtxStack[mymode].position)
	{
		//was commented out before zero sep modifications
		//handled in matrix now
		//MMU_new.gxstat.se = 1;
	//	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600, gxstat);
	//	return;
	}
	

	MatrixCopy(mtxCurrent[mymode], MatrixStackPopMatrix (&mtxStack[mymode], i));

	GFX_DELAY(36);

	if (mymode == 2)
		MatrixCopy(mtxCurrent[1], MatrixStackPopMatrix (&mtxStack[1], i));

	//gxstat |= ((mtxStack[0].position << 13) | (mtxStack[1].position << 8));
}

static void gfx3d_glStoreMatrix(u32 v)
{
	//this command always works on both pos and vector when either pos or pos-vector are the current mtx mode
	short mymode = (mode==1?2:mode);

	//limit height of these stacks.
	//without the mymode==3 namco classics galaxian will try to use pos=1 and overrun the stack, corrupting emu
	if(mymode==0 || mymode==3)
		v = 0;

	if (v > 31) return;

	MatrixStackLoadMatrix (&mtxStack[mymode], v, mtxCurrent[mymode]);

	GFX_DELAY(17);

	if(mymode==2)
		MatrixStackLoadMatrix (&mtxStack[1], v, mtxCurrent[1]);
}

static void gfx3d_glRestoreMatrix(u32 v)
{
	//this command always works on both pos and vector when either pos or pos-vector are the current mtx mode
	short mymode = (mode==1?2:mode);

	//limit height of these stacks
	//without the mymode==3 namco classics galaxian will try to use pos=1 and overrun the stack, corrupting emu
	if(mymode==0 || mymode==3)
		v = 0;

	if (v > 31) return;

	MatrixCopy (mtxCurrent[mymode], MatrixStackGetPos(&mtxStack[mymode], v));

	GFX_DELAY(36);

	if (mymode == 2)
		MatrixCopy (mtxCurrent[1], MatrixStackGetPos(&mtxStack[1], v));

	//printf("restore: matrix %d to: \n",mymode); MatrixPrint(mtxCurrent[1]);

}

static void gfx3d_glLoadIdentity()
{
	MatrixIdentity (mtxCurrent[mode]);

	GFX_DELAY(19);

	if (mode == 2)
		MatrixIdentity (mtxCurrent[1]);

	//printf("identity: %d to: \n",mode); MatrixPrint(mtxCurrent[1]);
}

static BOOL gfx3d_glLoadMatrix4x4(s32 v)
{
	mtxCurrent[mode][ML4x4ind] = (float)v;

	++ML4x4ind;
	if(ML4x4ind<16) return FALSE;
	ML4x4ind = 0;

	GFX_DELAY(19);

	vector_fix2float<4>(mtxCurrent[mode], 4096.f);

	if (mode == 2)
		MatrixCopy (mtxCurrent[1], mtxCurrent[2]);

	//printf("load4x4: matrix %d to: \n",mode); MatrixPrint(mtxCurrent[1]);
	return TRUE;
}

static BOOL gfx3d_glLoadMatrix4x3(s32 v)
{
	mtxCurrent[mode][ML4x3ind] = (float)v;

	ML4x3ind++;
	if((ML4x3ind & 0x03) == 3) ML4x3ind++;
	if(ML4x3ind<16) return FALSE;
	ML4x3ind = 0;

	vector_fix2float<4>(mtxCurrent[mode], 4096.f);

	//fill in the unusued matrix values
	mtxCurrent[mode][3] = mtxCurrent[mode][7] = mtxCurrent[mode][11] = 0.f;
	mtxCurrent[mode][15] = 1.f;

	GFX_DELAY(30);

	if (mode == 2)
		MatrixCopy (mtxCurrent[1], mtxCurrent[2]);
	//printf("load4x3: matrix %d to: \n",mode); MatrixPrint(mtxCurrent[1]);
	return TRUE;
}

static BOOL gfx3d_glMultMatrix4x4(s32 v)
{
	mtxTemporal[MM4x4ind] = (float)v;

	MM4x4ind++;
	if(MM4x4ind<16) return FALSE;
	MM4x4ind = 0;

	GFX_DELAY(35);

	vector_fix2float<4>(mtxTemporal, 4096.f);

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);

	if (mode == 2)
	{
		MatrixMultiply (mtxCurrent[1], mtxTemporal);
		GFX_DELAY_M2(30);
	}

	//printf("mult4x4: matrix %d to: \n",mode); MatrixPrint(mtxCurrent[1]);

	MatrixIdentity (mtxTemporal);
	return TRUE;
}

static BOOL gfx3d_glMultMatrix4x3(s32 v)
{
	mtxTemporal[MM4x3ind] = (float)v;

	MM4x3ind++;
	if((MM4x3ind & 0x03) == 3) MM4x3ind++;
	if(MM4x3ind<16) return FALSE;
	MM4x3ind = 0;

	GFX_DELAY(31);

	vector_fix2float<4>(mtxTemporal, 4096.f);

	//fill in the unusued matrix values
	mtxTemporal[3] = mtxTemporal[7] = mtxTemporal[11] = 0.f;
	mtxTemporal[15] = 1.f;

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);

	if (mode == 2)
	{
		MatrixMultiply (mtxCurrent[1], mtxTemporal);
		GFX_DELAY_M2(30);
	}

	//printf("mult4x3: matrix %d to: \n",mode); MatrixPrint(mtxCurrent[1]);

	//does this really need to be done?
	MatrixIdentity (mtxTemporal);
	return TRUE;
}

static BOOL gfx3d_glMultMatrix3x3(s32 v)
{
	mtxTemporal[MM3x3ind] = (float)v;


	MM3x3ind++;
	if((MM3x3ind & 0x03) == 3) MM3x3ind++;
	if(MM3x3ind<12) return FALSE;
	MM3x3ind = 0;

	GFX_DELAY(28);

	vector_fix2float<3>(mtxTemporal, 4096.f);

	//fill in the unusued matrix values
	mtxTemporal[3] = mtxTemporal[7] = mtxTemporal[11] = 0;
	mtxTemporal[15] = 1;
	mtxTemporal[12] = mtxTemporal[13] = mtxTemporal[14] = 0;

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);

	if (mode == 2)
	{
		MatrixMultiply (mtxCurrent[1], mtxTemporal);
		GFX_DELAY_M2(30);
	}

	//printf("mult3x3: matrix %d to: \n",mode); MatrixPrint(mtxCurrent[1]);


	//does this really need to be done?
	MatrixIdentity (mtxTemporal);
	return TRUE;
}

static BOOL gfx3d_glScale(s32 v)
{
	scale[scaleind] = fix2float(v);

	++scaleind;

	if(scaleind<3) return FALSE;
	scaleind = 0;

	MatrixScale (mtxCurrent[(mode==2?1:mode)], scale);
	//printf("scale: matrix %d to: \n",mode); MatrixPrint(mtxCurrent[1]);

	GFX_DELAY(22);

	//note: pos-vector mode should not cause both matrices to scale.
	//the whole purpose is to keep the vector matrix orthogonal
	//so, I am leaving this commented out as an example of what not to do.
	//if (mode == 2)
	//	MatrixScale (mtxCurrent[1], scale);
	return TRUE;
}

static BOOL gfx3d_glTranslate(s32 v)
{
	trans[transind] = fix2float(v);

	++transind;

	if(transind<3) return FALSE;
	transind = 0;

	MatrixTranslate (mtxCurrent[mode], trans);

	GFX_DELAY(22);

	if (mode == 2)
	{
		MatrixTranslate (mtxCurrent[1], trans);
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

static void gfx3d_glNormal(u32 v)
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

		for(i=0; i<4; i++)
		{
			if(!((lightMask>>i)&1)) continue;

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

		for(c=0;c<3;c++)
			colorRGB[c] = std::min(31,vertexColor[c]);
	}

	GFX_DELAY(9);
	GFX_DELAY_M2((lightMask) & 0x01);
	GFX_DELAY_M2((lightMask>>1) & 0x01);
	GFX_DELAY_M2((lightMask>>2) & 0x01);
	GFX_DELAY_M2((lightMask>>3) & 0x01);
}

static void gfx3d_glTexCoord(u32 val)
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
	GFX_DELAY(1);
}

static BOOL gfx3d_glVertex16b(unsigned int v)
{
	if(coordind==0)
	{
		//coord[0]		= float16table[v&0xFFFF];
		//coord[1]		= float16table[v>>16];
		u16coord[0] = v&0xFFFF;
		u16coord[1] = (v>>16)&0xFFFF;

		++coordind;
		return FALSE;
	}

	//coord[2]	  = float16table[v&0xFFFF];
	u16coord[2] = v&0xFFFF;

	coordind = 0;
	SetVertex ();

	GFX_DELAY(9);
	return TRUE;
}

static void gfx3d_glVertex10b(u32 v)
{
	//coord[0]		= float10Table[v&1023];
	//coord[1]		= float10Table[(v>>10)&1023];
	//coord[2]		= float10Table[(v>>20)&1023];
	u16coord[0] = (v&1023)<<6;
	u16coord[1] = ((v>>10)&1023)<<6;
	u16coord[2] = ((v>>20)&1023)<<6;

	GFX_DELAY(8);
	SetVertex ();
}

static void gfx3d_glVertex3_cord(unsigned int one, unsigned int two, unsigned int v)
{
	//coord[one]		= float16table[v&0xffff];
	//coord[two]		= float16table[v>>16];
	u16coord[one]		= v&0xffff;
	u16coord[two]		= (v>>16)&0xFFFF;

	SetVertex ();

	GFX_DELAY(8);
}

static void gfx3d_glVertex_rel(u32 v)
{
	//coord[0]		+= float10RelTable[v&1023];
	//coord[1]		+= float10RelTable[(v>>10)&1023];
	//coord[2]		+= float10RelTable[(v>>20)&1023];

	u16coord[0] += (u16)(((s16)((v&1023)<<6))>>6);
	u16coord[1] += (u16)(((s16)(((v>>10)&1023)<<6))>>6);
	u16coord[2] += (u16)(((s16)(((v>>20)&1023)<<6))>>6);


	SetVertex ();

	GFX_DELAY(8);
}

static void gfx3d_glPolygonAttrib (u32 val)
{
	if(inBegin) {
		//PROGINFO("Set polyattr in the middle of a begin/end pair.\n  (This won't be activated until the next begin)\n");
		//TODO - we need some some similar checking for teximageparam etc.
	}
	polyAttrPending = val;
	GFX_DELAY(1);
}

static void gfx3d_glTexImage(u32 val)
{
	textureFormat = val;
	gfx3d_glTexImage_cache();
	GFX_DELAY(1);
}

static void gfx3d_glTexPalette(u32 val)
{
	texturePalette = val;
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
static void gfx3d_glLightDirection (u32 v)
{
	int index = v>>30;

	lightDirection[index] = v;
	gfx3d_glLightDirection_cache(index);
	GFX_DELAY(6);
}

static void gfx3d_glLightColor (u32 v)
{
	int index = v>>30;
	lightColor[index] = v;
	GFX_DELAY(1);
}

static BOOL gfx3d_glShininess (u32 val)
{
	shininessTable[shininessInd++] = ((val & 0xFF) / 256.0f);
	shininessTable[shininessInd++] = (((val >> 8) & 0xFF) / 256.0f);
	shininessTable[shininessInd++] = (((val >> 16) & 0xFF) / 256.0f);
	shininessTable[shininessInd++] = (((val >> 24) & 0xFF) / 256.0f);

	if (shininessInd < 128) return FALSE;
	shininessInd = 0;
	GFX_DELAY(32);
	return TRUE;
}

static void gfx3d_glBegin(u32 v)
{
	inBegin = TRUE;
	vtxFormat = v&0x03;
	triStripToggle = 0;
	tempVertInfo.count = 0;
	tempVertInfo.first = true;
	polyAttr = polyAttrPending;
	gfx3d_glPolygonAttrib_cache();
	GFX_DELAY(1);
}

static void gfx3d_glEnd(void)
{
	inBegin = FALSE;
	tempVertInfo.count = 0;
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
	MMU_new.gxstat.tr = 0;		// clear boxtest bit
	MMU_new.gxstat.tb = 1;		// busy

	BTcoords[BTind++] = v & 0xFFFF;
	BTcoords[BTind++] = v >> 16;

	if (BTind < 5) return FALSE;
	BTind = 0;

	MMU_new.gxstat.tb = 0;		// clear busy
	GFX_DELAY(103);

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
	VERT verts[8];
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
	GFX3D_Clipper::TClippedPoly tempClippedPoly;
	boxtestClipper.clippedPolyCounter = 0;
	boxtestClipper.clippedPolys = &tempClippedPoly;

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
	for(int i=0;i<8;i++) {
		//MatrixMultVec4x4_M2(mtxCurrent[0], verts[i].coord);

		//yuck.. cant use the sse2 accelerated ones because vert.coords is not cache aligned or something
		//i dunno
		
		void _NOSSE_MatrixMultVec4x4 (const float *matrix, float *vecPtr);
		_NOSSE_MatrixMultVec4x4(mtxCurrent[1],verts[i].coord);
		_NOSSE_MatrixMultVec4x4(mtxCurrent[0],verts[i].coord);
	}

	//clip each poly
	for(int i=0;i<6;i++) 
	{
		POLY* poly = &polys[i];
		VERT* vertTable[4] = {
			&verts[poly->vertIndexes[0]],
			&verts[poly->vertIndexes[1]],
			&verts[poly->vertIndexes[2]],
			&verts[poly->vertIndexes[3]]
		};

		boxtestClipper.clipPoly(poly,vertTable);
		
		//if any portion of this poly was retained, then the test passes.
		if(boxtestClipper.clippedPolyCounter>0) {
			//printf("%06d PASS %d\n",boxcounter,gxFIFO.size);
			MMU_new.gxstat.tr = 1;
			break;
		}
	}

	if(MMU_new.gxstat.tr == 0)
	{
		//printf("%06d FAIL %d\n",boxcounter,gxFIFO.size);
	}
	
	return TRUE;
}

static BOOL gfx3d_glPosTest(u32 v)
{
	//this is apparently tested by transformers decepticons and ultimate spiderman

	//printf("POSTEST\n");
#ifdef TESTS_ENABLED
	MMU_new.gxstat.tb = 1;
#endif

	PTcoords[PTind++] = float16table[v & 0xFFFF];
	PTcoords[PTind++] = float16table[v >> 16];

	if (PTind < 3) return FALSE;
	PTind = 0;
	
	PTcoords[3] = 1.0f;

	MatrixMultVec4x4(mtxCurrent[1], PTcoords);
	MatrixMultVec4x4(mtxCurrent[0], PTcoords);

	MMU_new.gxstat.tb = 0;

	GFX_DELAY(9);

	return TRUE;
}

static void gfx3d_glVecTest(u32 v)
{
	GFX_DELAY(5);

	printf("VECTEST\n");

	CACHE_ALIGN float normal[4] = { normalTable[v&1023],
						normalTable[(v>>10)&1023],
						normalTable[(v>>20)&1023],
						1};

	MatrixMultVec4x4(mtxCurrent[2], normal);

	s16 x = (s16)(normal[0]);
	s16 y = (s16)(normal[1]);
	s16 z = (s16)(normal[2]);

	MMU_new.gxstat.tb = 0;		// clear busy
	T1WriteWord(MMU.MMU_MEM[0][0x40], 0x630, x);
	T1WriteWord(MMU.MMU_MEM[0][0x40], 0x632, y);
	T1WriteWord(MMU.MMU_MEM[0][0x40], 0x634, z);

}
//================================================================================= Geometry Engine
//================================================================================= (end)
//=================================================================================

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
	gfx3d.fogColor = v;
}

void gfx3d_glFogOffset (u32 v)
{
	gfx3d.fogOffset = (v&0x7fff);
}

void gfx3d_glClearDepth(u32 v)
{
	v &= 0x7FFF;
	gfx3d.clearDepth = gfx3d_extendDepth_15_to_24(v);
}

// Ignored for now
void gfx3d_glSwapScreen(unsigned int screen)
{
}

int gfx3d_GetNumPolys()
{
	//so is this in the currently-displayed or currently-built list?
	return (polylists[listTwiddle].count);
}

int gfx3d_GetNumVertex()
{
	//so is this in the currently-displayed or currently-built list?
	return (vertlists[listTwiddle].count);
}

void gfx3d_UpdateToonTable(u8 offset, u16 val)
{
	gfx3d.u16ToonTable[offset] =  val;
}

void gfx3d_UpdateToonTable(u8 offset, u32 val)
{
	//C.O.P. sets toon table via this method
	gfx3d.u16ToonTable[offset] = val & 0xFFFF;
	gfx3d.u16ToonTable[offset+1] = val >> 16;
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

void gfx3d_ClearStack()
{
	MatrixStackSetStackPosition(&mtxStack[0], -5);
	//MatrixStackSetStackPosition(&mtxStack[1], -55);
	//MatrixStackSetStackPosition(&mtxStack[2], -55); //?
	MatrixStackSetStackPosition(&mtxStack[3], -5);
}

void gfx3d_glAlphaFunc(u32 v)
{
	gfx3d.alphaTestRef = v&31;
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

//#define _3D_LOG_EXEC
static void gfx3d_execute(u8 cmd, u32 param)
{
	//printf("*** gxFIFO: exec 0x%02X, size %03i\n", cmd, gxFIFO.size);
#ifdef _3D_LOG_EXEC
	u32 gxstat2 = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600);
	INFO("*** gxFIFO: exec 0x%02X, tail %03i, gxstat 0x%08X (timer %i)\n", cmd, gxFIFO.tail, gxstat2, nds_timer);
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

void gfx3d_execute3D()
{
	u8	cmd = 0;
	u32	param = 0;

#ifndef FLUSHMODE_HACK
	if (isSwapBuffers) return;
#endif

	//this is a SPEED HACK
	//fifo is currently emulated more accurately than it probably needs to be.
	//without this batch size the emuloop will escape way too often to run fast.
	const int HACK_FIFO_BATCH_SIZE = 64;

	for(int i=0;i<HACK_FIFO_BATCH_SIZE;i++) {
		if(GFX_PIPErecv(&cmd, &param))
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


	//i thought it might be right to move these here, but it didnt help.
	//maybe its a good idea for later.

	//if(gxFIFO.size <= 127)
	//{
	//	if(gxstat.gxfifo_irq == 1) 
	//		setIF(0, (1<<21)); //the half gxfifo irq
	//
	//	//might need to trigger a gxfifo dma
	//	triggerDma(EDMAMode_GXFifo);
	//}

	//if(gxFIFO.size == 0) {
	//	if(gxstat.gxfifo_irq == 2) 
	//	setIF(0, (1<<21)); //the empty gxfifo irq
	//}
}

void gfx3d_glFlush(u32 v)
{
	//printf("-------------FLUSH------------- (vcount=%d\n",nds.VCount);
	gfx3d.sortmode = BIT0(v);
	gfx3d.wbuffer = BIT1(v);
#if 0
	if (isSwapBuffers)
	{
		INFO("Error: swapBuffers already use\n");
	}
#endif
	
	isSwapBuffers = TRUE;
	
	//well, the game wanted us to flush.
	//it may be badly timed. lets just flush it.
#ifdef FLUSHMODE_HACK
	gfx3d_doFlush();
#endif

	GFX_DELAY(1);
}

static bool gfx3d_ysort_compare(int num1, int num2)
{
	const POLY &poly1 = polylist->list[num1];
	const POLY &poly2 = polylist->list[num2];

	//this may be verified by checking the game create menus in harvest moon island of happiness
	//also the buttons in the knights in the nightmare frontend depend on this and the perspective division
	if (poly1.maxy < poly2.maxy) return true;
	if (poly1.maxy > poly2.maxy) return false;
	if (poly1.miny > poly2.miny) return true;
	if (poly1.miny < poly2.miny) return false;
	//notably, the main shop interface in harvest moon will not have a correct RTN button
	//i think this is due to a math error rounding its position to one pixel too high and it popping behind
	//the bar that it sits on.
	//everything else in all the other menus that I could find looks right..

	//make sure we respect the game's ordering in cases of complete ties
	//this makes it a stable sort.
	//this must be a stable sort or else advance wars DOR will flicker in the main map mode
	if (num1 < num2) return true;
	else return false;
}

static void gfx3d_doFlush()
{
	gfx3d.frameCtr++;

	//the renderer will get the lists we just built
	gfx3d.polylist = polylist;
	gfx3d.vertlist = vertlist;

	//and also our current render state
	if(BIT1(control)) gfx3d.shading = GFX3D::HIGHLIGHT;
	else gfx3d.shading = GFX3D::TOON;
	gfx3d.enableTexturing = BIT0(control);
	gfx3d.enableAlphaTest = BIT2(control);
	gfx3d.enableAlphaBlending = BIT3(control);
	gfx3d.enableAntialiasing = BIT4(control);
	gfx3d.enableEdgeMarking = BIT5(control);
	gfx3d.enableFogAlphaOnly = BIT6(control);
	gfx3d.enableFog = BIT7(control);
	gfx3d.enableClearImage = BIT14(control);
	gfx3d.fogShift = (control>>8)&0xF;

	int polycount = polylist->count;

	//find the min and max y values for each poly.
	//TODO - this could be a small waste of time if we are manual sorting the translucent polys
	//TODO - this _MUST_ be moved later in the pipeline, after clipping.
	//the w-division here is just an approximation to fix the shop in harvest moon island of happiness
	//also the buttons in the knights in the nightmare frontend depend on this
	for(int i=0; i<polycount; i++)
	{
		POLY &poly = polylist->list[i];
		float verty = vertlist->list[poly.vertIndexes[0]].y;
		float vertw = vertlist->list[poly.vertIndexes[0]].w;
		verty = (verty+vertw)/(2*vertw);
		poly.miny = poly.maxy = verty;

		for(int j=1; j<poly.type; j++)
		{
			verty = vertlist->list[poly.vertIndexes[j]].y;
			vertw = vertlist->list[poly.vertIndexes[j]].w;
			verty = (verty+vertw)/(2*vertw);
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

	drawPending = TRUE;
}

void gfx3d_VBlankSignal()
{
	if (isSwapBuffers)
	{
#ifndef FLUSHMODE_HACK
		gfx3d_doFlush();
#endif
		GFX_DELAY(392);
		isSwapBuffers = FALSE;
	}
}

void gfx3d_VBlankEndSignal(bool skipFrame)
{
	if (!drawPending) return;
	if(skipFrame) return;

	drawPending = FALSE;

	//if the null 3d core is chosen, then we need to clear out the 3d buffers to keep old data from being rendered
	if(gpu3D == &gpu3DNull || !CommonSettings.showGpu.main)
	{
		memset(gfx3d_convertedScreen,0,sizeof(gfx3d_convertedScreen));
		return;
	}

	gpu3D->NDS_3D_Render();
}

//#define _3D_LOG

void gfx3d_sendCommandToFIFO(u32 val)
{
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



void gfx3d_Control(u32 v)
{
	control = v;
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

void gfx3d_GetLineData(int line, u8** dst)
{
	*dst = gfx3d_convertedScreen+((line)<<(8+2));
}

void gfx3d_GetLineData15bpp(int line, u16** dst)
{
	//TODO - this is not very thread safe!!!
	static u16 buf[256];
	*dst = buf;

	u8* lineData;
	gfx3d_GetLineData(line, &lineData);
	for(int i=0;i<256;i++)
	{
		const u8 r = lineData[i*4+0];
		const u8 g = lineData[i*4+1];
		const u8 b = lineData[i*4+2];
		const u8 a = lineData[i*4+3];
		buf[i] = R6G6B6TORGB15(r,g,b) | (a==0?0:0x8000);
	}
}


//http://www.opengl.org/documentation/specs/version1.1/glspec1.1/node17.html
//talks about the state required to process verts in quadlists etc. helpful ideas.
//consider building a little state structure that looks exactly like this describes

SFORMAT SF_GFX3D[]={
	{ "GCTL", 4, 1, &control}, // no longer regenerated indirectly, see comment in loadstate()
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
	{ "GSCO", 4, 1, u16coord},
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
	{ "GFLP", 4, 1, &flushPending},
	{ "GDRP", 4, 1, &drawPending},
	{ "GSET", 4, 1, &gfx3d.enableTexturing},
	{ "GSEA", 4, 1, &gfx3d.enableAlphaTest},
	{ "GSEB", 4, 1, &gfx3d.enableAlphaBlending},
	{ "GSEX", 4, 1, &gfx3d.enableAntialiasing},
	{ "GSEE", 4, 1, &gfx3d.enableEdgeMarking},
	{ "GSEC", 4, 1, &gfx3d.enableClearImage},
	{ "GSEF", 4, 1, &gfx3d.enableFog},
	{ "GSEO", 4, 1, &gfx3d.enableFogAlphaOnly},
	{ "GFSH", 4, 1, &gfx3d.fogShift},
	{ "GSSH", 4, 1, &gfx3d.shading},
	{ "GSWB", 4, 1, &gfx3d.wbuffer},
	{ "GSSM", 4, 1, &gfx3d.sortmode},
	{ "GSAR", 1, 1, &gfx3d.alphaTestRef},
	{ "GSVP", 4, 1, &viewport},
	{ "GSCC", 4, 1, &gfx3d.clearColor},
	{ "GSCD", 4, 1, &gfx3d.clearDepth},
	{ "GSFC", 4, 4, &gfx3d.fogColor},
	{ "GSFO", 4, 1, &gfx3d.fogOffset},
	{ "GST2", 2, 32, gfx3d.u16ToonTable},
	{ "GSST", 4, 128, shininessTable},
	{ "GSSI", 4, 1, &shininessInd},
	//------------------------
	{ "GTST", 4, 1, &triStripToggle},
	{ "GTVC", 4, 1, &tempVertInfo.count},
	{ "GTVM", 4, 4, tempVertInfo.map},
	{ "GTVF", 4, 1, &tempVertInfo.first},
	{ "G3CX", 1, 4*256*192, gfx3d_convertedScreen},
	{ 0 }
};

//-------------savestate
void gfx3d_savestate(EMUFILE* os)
{
	//version
	write32le(4,os);

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
		for(int j=0;j<mtxStack[i].size*16;j++)
			OSWRITE(mtxStack[i].matrix[j]);
	}

	gxf_hardware.savestate(os);

	// evidently these need to be saved because we don't cache the matrix that would need to be used to properly regenerate them
	OSWRITE(cacheLightDirection);
	OSWRITE(cacheHalfVector);
}

bool gfx3d_loadstate(EMUFILE* is, int size)
{
	int version;
	if(read32le(&version,is) != 1) return false;
	if(size==8) version = 0;


	gfx3d_glPolygonAttrib_cache();
	gfx3d_glTexImage_cache();
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
			for(int j=0;j<mtxStack[i].size*16;j++)
				OSREAD(mtxStack[i].matrix[j]);
		}
	}

	if(version>=3) {
		gxf_hardware.loadstate(is);
	}

	gfx3d.polylist = &polylists[listTwiddle^1];
	gfx3d.vertlist = &vertlists[listTwiddle^1];
	gfx3d.polylist->count=0;
	gfx3d.vertlist->count=0;

	if(version >= 4)
	{
		OSREAD(cacheLightDirection);
		OSREAD(cacheHalfVector);
	}

	return true;
}

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
static T interpolate(const float ratio, const T& x0, const T& x1) {
	return (T)(x0 + (float)(x1-x0) * (ratio));
}


//http://www.cs.berkeley.edu/~ug/slide/pipeline/assignments/as6/discussion.shtml
#ifdef OPTIMIZED_CLIPPING_METHOD
template<int coord, int which> static FORCEINLINE VERT clipPoint(VERT* inside, VERT* outside)
#else
static FORCEINLINE VERT clipPoint(VERT* inside, VERT* outside, int coord, int which)
#endif
{
	VERT ret;

	float coord_inside = inside->coord[coord];
	float coord_outside = outside->coord[coord];
	float w_inside = inside->coord[3];
	float w_outside = outside->coord[3];

	float t;

	if(which==-1) {
		w_outside = -w_outside;
		w_inside = -w_inside;
	}
	
	t = (coord_inside - w_inside)/ ((w_outside-w_inside) - (coord_outside-coord_inside));
	

#define INTERP(X) ret . X = interpolate(t, inside-> X ,outside-> X )
	
	INTERP(coord[0]); INTERP(coord[1]); INTERP(coord[2]); INTERP(coord[3]);
	INTERP(texcoord[0]); INTERP(texcoord[1]);

	if(CommonSettings.GFX3D_HighResolutionInterpolateColor)
	{
		INTERP(fcolor[0]); INTERP(fcolor[1]); INTERP(fcolor[2]);
	}
	else
	{
		INTERP(color[0]); INTERP(color[1]); INTERP(color[2]);
		ret.color_to_float();
	}

	//this seems like a prudent measure to make sure that math doesnt make a point pop back out
	//of the clip volume through interpolation
	if(which==-1)
		ret.coord[coord] = -ret.coord[3];
	else
		ret.coord[coord] = ret.coord[3];

	return ret;
}

#ifdef OPTIMIZED_CLIPPING_METHOD

#define MAX_SCRATCH_CLIP_VERTS (4*6 + 40)
static VERT scratchClipVerts [MAX_SCRATCH_CLIP_VERTS];
static int numScratchClipVerts = 0;

template <int coord, int which, class Next>
class ClipperPlane
{
public:
	ClipperPlane(Next& next) : m_next(next) {}

	void init(VERT* verts)
	{
		m_prevVert =  NULL;
		m_firstVert = NULL;
		m_next.init(verts);
	}

	void clipVert(VERT* vert)
	{
		if(m_prevVert)
			this->clipSegmentVsPlane(m_prevVert, vert);
		else
			m_firstVert = vert;
		m_prevVert = vert;
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
	Next& m_next;

	FORCEINLINE void clipSegmentVsPlane(VERT* vert0, VERT* vert1)
	{
		float* vert0coord = vert0->coord;
		float* vert1coord = vert1->coord;
		bool out0, out1;
		if(which==-1) 
			out0 = vert0coord[coord] < -vert0coord[3];
		else 
			out0 = vert0coord[coord] > vert0coord[3];
		if(which==-1) 
			out1 = vert1coord[coord] < -vert1coord[3];
		else
			out1 = vert1coord[coord] > vert1coord[3];

		//CONSIDER: should we try and clip things behind the eye? does this code even successfully do it? not sure.
		//if(coord==2 && which==1) {
		//	out0 = vert0coord[2] < 0;
		//	out1 = vert1coord[2] < 0;
		//}

		//both outside: insert no points
		if(out0 && out1) {
			CLIPLOG(" both outside\n");
		}

		//both inside: insert the next point
		if(!out0 && !out1) 
		{
			CLIPLOG(" both inside\n");
			m_next.clipVert(vert1);
		}

		//exiting volume: insert the clipped point
		if(!out0 && out1)
		{
			CLIPLOG(" exiting\n");
			assert((u32)numScratchClipVerts < MAX_SCRATCH_CLIP_VERTS);
			scratchClipVerts[numScratchClipVerts] = clipPoint<coord, which>(vert0,vert1);
			m_next.clipVert(&scratchClipVerts[numScratchClipVerts++]);
		}

		//entering volume: insert clipped point and the next (interior) point
		if(out0 && !out1) {
			CLIPLOG(" entering\n");
			assert((u32)numScratchClipVerts < MAX_SCRATCH_CLIP_VERTS);
			scratchClipVerts[numScratchClipVerts] = clipPoint<coord, which>(vert1,vert0);
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
	void clipVert(VERT* vert)
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
static ClipperOutput clipperOut;
typedef ClipperPlane<2, 1,ClipperOutput> Stage6; static Stage6 clipper6 (clipperOut); // back plane //TODO - we need to parameterize back plane clipping
typedef ClipperPlane<2,-1,Stage6> Stage5;        static Stage5 clipper5 (clipper6); // front plane
typedef ClipperPlane<1, 1,Stage5> Stage4;        static Stage4 clipper4 (clipper5); // top plane
typedef ClipperPlane<1,-1,Stage4> Stage3;        static Stage3 clipper3 (clipper4); // bottom plane
typedef ClipperPlane<0, 1,Stage3> Stage2;        static Stage2 clipper2 (clipper3); // right plane
typedef ClipperPlane<0,-1,Stage2> Stage1;        static Stage1 clipper  (clipper2); // left plane

void GFX3D_Clipper::clipPoly(POLY* poly, VERT** verts)
{
	CLIPLOG("==Begin poly==\n");

	int type = poly->type;
	numScratchClipVerts = 0;

	clipper.init(clippedPolys[clippedPolyCounter].clipVerts);
	for(int i=0;i<type;i++)
		clipper.clipVert(verts[i]);
	int outType = clipper.finish();

	assert((u32)outType < MAX_CLIPPED_VERTS);
	if(outType < 3)
	{
		//a totally clipped poly. discard it.
		//or, a degenerate poly. we're not handling these right now
	}
	else
	{
		clippedPolys[clippedPolyCounter].type = outType;
		clippedPolys[clippedPolyCounter].poly = poly;
		clippedPolyCounter++;
	}
}

void GFX3D_Clipper::clipSegmentVsPlane(VERT** verts, const int coord, int which)
{
	// not used (it's probably ok to delete this function)
	assert(0);
}

void GFX3D_Clipper::clipPolyVsPlane(const int coord, int which)
{
	// not used (it's probably ok to delete this function)
	assert(0);
}

#else // if not OPTIMIZED_CLIPPING_METHOD:

FORCEINLINE void GFX3D_Clipper::clipSegmentVsPlane(VERT** verts, const int coord, int which)
{
	bool out0, out1;
	if(which==-1) 
		out0 = verts[0]->coord[coord] < -verts[0]->coord[3];
	else 
		out0 = verts[0]->coord[coord] > verts[0]->coord[3];
	if(which==-1) 
		out1 = verts[1]->coord[coord] < -verts[1]->coord[3];
	else
		out1 = verts[1]->coord[coord] > verts[1]->coord[3];

	//CONSIDER: should we try and clip things behind the eye? does this code even successfully do it? not sure.
	//if(coord==2 && which==1) {
	//	out0 = verts[0]->coord[2] < 0;
	//	out1 = verts[1]->coord[2] < 0;
	//}

	//both outside: insert no points
	if(out0 && out1) {
		CLIPLOG(" both outside\n");
	}

	//both inside: insert the first point
	if(!out0 && !out1) 
	{
		CLIPLOG(" both inside\n");
		outClippedPoly.clipVerts[outClippedPoly.type++] = *verts[1];
	}

	//exiting volume: insert the clipped point and the first (interior) point
	if(!out0 && out1)
	{
		CLIPLOG(" exiting\n");
		outClippedPoly.clipVerts[outClippedPoly.type++] = clipPoint(verts[0],verts[1], coord, which);
	}

	//entering volume: insert clipped point
	if(out0 && !out1) {
		CLIPLOG(" entering\n");
		outClippedPoly.clipVerts[outClippedPoly.type++] = clipPoint(verts[1],verts[0], coord, which);
		outClippedPoly.clipVerts[outClippedPoly.type++] = *verts[1];

	}
}

FORCEINLINE void GFX3D_Clipper::clipPolyVsPlane(const int coord, int which)
{
	outClippedPoly.type = 0;
	CLIPLOG2("Clipping coord %d against %f\n",coord,x);
	for(int i=0;i<tempClippedPoly.type;i++)
	{
		VERT* testverts[2] = {&tempClippedPoly.clipVerts[i],&tempClippedPoly.clipVerts[(i+1)%tempClippedPoly.type]};
		clipSegmentVsPlane(testverts, coord, which);
	}

	//this doesnt seem to help any. leaving it until i decide to get rid of it
	//int j = index_start_table[tempClippedPoly.type-3];
	//for(int i=0;i<tempClippedPoly.type;i++,j+=2)
	//{
	//	VERT* testverts[2] = {&tempClippedPoly.clipVerts[index_lookup_table[j]],&tempClippedPoly.clipVerts[index_lookup_table[j+1]]};
	//	clipSegmentVsPlane(testverts, coord, which);
	//}

	tempClippedPoly = outClippedPoly;
}


void GFX3D_Clipper::clipPoly(POLY* poly, VERT** verts)
{
	int type = poly->type;

	CLIPLOG("==Begin poly==\n");

	tempClippedPoly.clipVerts[0] = *verts[0];
	tempClippedPoly.clipVerts[1] = *verts[1];
	tempClippedPoly.clipVerts[2] = *verts[2];
	if(type==4)
		tempClippedPoly.clipVerts[3] = *verts[3];

	
	tempClippedPoly.type = type;

	clipPolyVsPlane(0, -1); 
	clipPolyVsPlane(0, 1);
	clipPolyVsPlane(1, -1);
	clipPolyVsPlane(1, 1);
	clipPolyVsPlane(2, -1);
	clipPolyVsPlane(2, 1);
	//TODO - we need to parameterize back plane clipping

	
	if(tempClippedPoly.type < 3)
	{
		//a totally clipped poly. discard it.
		//or, a degenerate poly. we're not handling these right now
	}
	else
	{
		//TODO - build right in this list instead of copying
		clippedPolys[clippedPolyCounter] = tempClippedPoly;
		clippedPolys[clippedPolyCounter].poly = poly;
		clippedPolyCounter++;
	}

}
#endif

