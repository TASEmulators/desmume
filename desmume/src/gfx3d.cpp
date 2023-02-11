/*	
	Copyright (C) 2006 yopyop
	Copyright (C) 2008-2023 DeSmuME team

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
#include "GPU_Operations.h"
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
static GFX3D_IOREG *_GFX3D_IORegisterMap = NULL;
Viewer3D_State viewer3D;

static LegacyGFX3DStateSFormat _legacyGFX3DStateSFormatPending;
static LegacyGFX3DStateSFormat _legacyGFX3DStateSFormatApplied;

//tables that are provided to anyone
CACHE_ALIGN u32 dsDepthExtend_15bit_to_24bit[32768];

// Matrix stack handling
CACHE_ALIGN NDSMatrixStack1  mtxStackProjection;
CACHE_ALIGN NDSMatrixStack32 mtxStackPosition;
CACHE_ALIGN NDSMatrixStack32 mtxStackPositionVector;
CACHE_ALIGN NDSMatrixStack1  mtxStackTexture;

u32 mtxStackIndex[4];

static CACHE_ALIGN s32 _mtxCurrent[4][16];
static CACHE_ALIGN s32 _mtxTemporal[16];
static MatrixMode _mtxMode = MATRIXMODE_PROJECTION;

// Indexes for matrix loading/multiplication
static u8 _ML4x4ind = 0;
static u8 _ML4x3ind = 0;
static u8 _MM4x4ind = 0;
static u8 _MM4x3ind = 0;
static u8 _MM3x3ind = 0;

// Data for vertex submission
static CACHE_ALIGN VertexCoord16x4 _vtxCoord16 = {0, 0, 0, 0};
static u8 _vtxCoord16CurrentIndex = 0;
static PolygonPrimitiveType _vtxFormat = GFX3D_TRIANGLES;
static bool _inBegin = false;

// Data for basic transforms
static CACHE_ALIGN Vector32x4 _regTranslate = {0, 0, 0, 0};
static u8 _regTranslateCurrentIndex = 0;
static CACHE_ALIGN Vector32x4 _regScale = {0, 0, 0, 0};
static u8 _regScaleCurrentIndex = 0;

//various other registers
static VertexCoord32x2 _regTexCoord = {0, 0};
static VertexCoord32x2 _texCoordTransformed = {0, 0};

u32 isSwapBuffers = FALSE;

static u32 _BTind = 0;
static u32 _PTind = 0;
static CACHE_ALIGN u16 _BTcoords[6] = {0, 0, 0, 0, 0, 0};
static CACHE_ALIGN VertexCoord32x4 _PTcoords = {0, 0, 0, (s32)(1<<12)};

//raw ds format poly attributes
static POLYGON_ATTR _polyAttrInProcess;
static POLYGON_ATTR _currentPolyAttr;
static TEXIMAGE_PARAM _currentPolyTexParam;
static u32 _currentPolyTexPalette = 0;

//the current vertex color, 5bit values
static FragmentColor _vtxColor555X = {31, 31, 31, 0};

//light state:
static u32 _regLightColor[4] = {0, 0, 0, 0};
static u32 _regLightDirection[4] = {0, 0, 0, 0};
//material state:
static u16 _regDiffuse  = 0;
static u16 _regAmbient  = 0;
static u16 _regSpecular = 0;
static u16 _regEmission = 0;

//used for indexing the shininess table during parameters to shininess command
static u8 _shininessTableCurrentIndex = 0;

// "Freelook" related things
int freelookMode = 0;
s32 freelookMatrix[16];

//-----------cached things:
//these dont need to go into the savestate. they can be regenerated from HW registers
//from polygonattr:
static u32 _lightMask = 0;
//other things:
static TextureTransformationMode _texCoordTransformMode = TextureTransformationMode_None;
static CACHE_ALIGN Vector32x4 _cacheLightDirection[4];
static CACHE_ALIGN Vector32x4 _cacheHalfVector[4];
//------------------

#define RENDER_FRONT_SURFACE 0x80
#define RENDER_BACK_SURFACE 0X40

static int _polygonListCompleted = 0;
static u8 _triStripToggle;

//list-building state
struct PolygonGenerationInfo
{
	size_t vtxCount;  // the number of vertices registered in this list
	u16 vtxIndex[4]; // indices to the main vert list
	bool isFirstPolyCompleted; // indicates that the first poly in a list has been completed
};
typedef struct PolygonGenerationInfo PolygonGenerationInfo;

static PolygonGenerationInfo _polyGenInfo;

static bool _isDrawPending = false;
//------------------------------------------------------------

static void makeTables()
{
	for (size_t i = 0; i < 32768; i++)
	{
		// 15-bit to 24-bit depth formula from http://problemkaputt.de/gbatek.htm#ds3drearplane
		//dsDepthExtend_15bit_to_24bit[i] = LE_TO_LOCAL_32( (i*0x0200) + ((i+1)>>15)*0x01FF );
		
		// Is GBATEK actually correct here? Let's try using a simplified formula and see if it's
		// more accurate.
		dsDepthExtend_15bit_to_24bit[i] = (u32)((i * 0x0200) + 0x01FF);
	}
}

GFX3D_Viewport GFX3D_ViewportParse(const u32 inValue)
{
	const IOREG_VIEWPORT reg = { inValue };
	GFX3D_Viewport outViewport;
	
	//I'm 100% sure this is basically 99% correct
	//the modular math is right. the details of how the +1 is handled may be wrong (this might should be dealt with in the viewport transformation instead)
	//Its an off by one error in any event so we may never know
	outViewport.width  = (u8)(reg.X2 - reg.X1) + 1;
	outViewport.height = (u8)(reg.Y2 - reg.Y1) + 1;
	outViewport.x      = (s16)reg.X1;
	
	//test: homie rollerz character select chooses nonsense for Y. they did the math backwards. their goal was a fullscreen viewport, they just messed up.
	//they also messed up the width...
	
	// The maximum viewport y-value is 191. Values above 191 need to wrap
	// around and go negative.
	//
	// Test case: The Homie Rollerz character select screen sets the y-value
	// to 253, which then wraps around to -2.
	outViewport.y      = (reg.Y1 > 191) ? ((s16)reg.Y1 - 0x00FF) : (s16)reg.Y1;
	
	return outViewport;
}

void GFX3D_SaveStatePOLY(const POLY &p, EMUFILE &os)
{
	os.write_32LE((u32)p.type);
	os.write_16LE(p.vertIndexes[0]);
	os.write_16LE(p.vertIndexes[1]);
	os.write_16LE(p.vertIndexes[2]);
	os.write_16LE(p.vertIndexes[3]);
	os.write_32LE(p.attribute.value);
	os.write_32LE(p.texParam.value);
	os.write_32LE(p.texPalette);
}

void GFX3D_LoadStatePOLY(POLY &p, EMUFILE &is)
{
	u32 polyType32;
	is.read_32LE(polyType32);
	p.type = (PolygonType)polyType32;
	
	is.read_16LE(p.vertIndexes[0]);
	is.read_16LE(p.vertIndexes[1]);
	is.read_16LE(p.vertIndexes[2]);
	is.read_16LE(p.vertIndexes[3]);
	is.read_32LE(p.attribute.value);
	is.read_32LE(p.texParam.value);
	is.read_32LE(p.texPalette);
}

void GFX3D_SaveStateVERT(const VERT &vtx, EMUFILE &os)
{
	os.write_floatLE(vtx.x);
	os.write_floatLE(vtx.y);
	os.write_floatLE(vtx.z);
	os.write_floatLE(vtx.w);
	os.write_floatLE(vtx.u);
	os.write_floatLE(vtx.v);
	os.write_u8(vtx.color[0]);
	os.write_u8(vtx.color[1]);
	os.write_u8(vtx.color[2]);
	os.write_floatLE(vtx.fcolor[0]);
	os.write_floatLE(vtx.fcolor[1]);
	os.write_floatLE(vtx.fcolor[2]);
}

void GFX3D_LoadStateVERT(VERT &vtx, EMUFILE &is)
{
	is.read_floatLE(vtx.x);
	is.read_floatLE(vtx.y);
	is.read_floatLE(vtx.z);
	is.read_floatLE(vtx.w);
	is.read_floatLE(vtx.u);
	is.read_floatLE(vtx.v);
	is.read_u8(vtx.color[0]);
	is.read_u8(vtx.color[1]);
	is.read_u8(vtx.color[2]);
	is.read_floatLE(vtx.fcolor[0]);
	is.read_floatLE(vtx.fcolor[1]);
	is.read_floatLE(vtx.fcolor[2]);
}

void gfx3d_init()
{
	_GFX3D_IORegisterMap = (GFX3D_IOREG *)(&MMU.ARM9_REG[0x0320]);
	
	_polyAttrInProcess.value = 0;
	_currentPolyAttr.value = 0;
	_currentPolyTexParam.value = 0;
	
	gxf_hardware.reset();
	
	makeTables();
	Render3D_Init();
}

void gfx3d_deinit()
{
	Render3D_DeInit();
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

	memset(&viewer3D, 0, sizeof(Viewer3D_State));
	
	gxf_hardware.reset();

	_isDrawPending = false;
	
	memset(&gfx3d, 0, sizeof(GFX3D));
	
	gfx3d.pendingState.DISP3DCNT.EnableTexMapping = 1;
	gfx3d.pendingState.DISP3DCNT.PolygonShading = PolygonShadingMode_Toon;
	gfx3d.pendingState.DISP3DCNT.EnableAlphaTest = 1;
	gfx3d.pendingState.DISP3DCNT.EnableAlphaBlending = 1;
	gfx3d.pendingState.clearDepth = DS_DEPTH15TO24(0x7FFF);
	
	gfx3d.appliedState.DISP3DCNT.EnableTexMapping = 1;
	gfx3d.appliedState.DISP3DCNT.PolygonShading = PolygonShadingMode_Toon;
	gfx3d.appliedState.DISP3DCNT.EnableAlphaTest = 1;
	gfx3d.appliedState.DISP3DCNT.EnableAlphaBlending = 1;
	gfx3d.appliedState.clearDepth = DS_DEPTH15TO24(0x7FFF);
	
	gfx3d.pendingListIndex = 0;
	gfx3d.appliedListIndex = 1;

	_polyAttrInProcess.value = 0;
	_currentPolyAttr.value = 0;
	_currentPolyTexParam.value = 0;
	_currentPolyTexPalette = 0;
	_mtxMode = MATRIXMODE_PROJECTION;
	_vtxCoord16.value = 0;
	_vtxCoord16CurrentIndex = 0;
	_vtxFormat = GFX3D_TRIANGLES;
	memset(&_regTranslate, 0, sizeof(_regTranslate));
	_regTranslateCurrentIndex = 0;
	memset(&_regScale, 0, sizeof(_regScale));
	_regScaleCurrentIndex = 0;
	memset(gxPIPE.cmd, 0, sizeof(gxPIPE.cmd));
	memset(gxPIPE.param, 0, sizeof(gxPIPE.param));
	_vtxColor555X.color = 0;
	memset(&_polyGenInfo, 0, sizeof(_polyGenInfo));
	memset(gfx3d.framebufferNativeSave, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u32));

	MatrixInit(_mtxCurrent[MATRIXMODE_PROJECTION]);
	MatrixInit(_mtxCurrent[MATRIXMODE_POSITION]);
	MatrixInit(_mtxCurrent[MATRIXMODE_POSITION_VECTOR]);
	MatrixInit(_mtxCurrent[MATRIXMODE_TEXTURE]);
	MatrixInit(_mtxTemporal);
	
	mtxStackIndex[MATRIXMODE_PROJECTION] = 0;
	mtxStackIndex[MATRIXMODE_POSITION] = 0;
	mtxStackIndex[MATRIXMODE_POSITION_VECTOR] = 0;
	mtxStackIndex[MATRIXMODE_TEXTURE] = 0;
	
	MatrixInit(mtxStackProjection[0]);
	for (size_t i = 0; i < NDSMATRIXSTACK_COUNT(MATRIXMODE_POSITION);        i++) { MatrixInit(mtxStackPosition[i]);       }
	for (size_t i = 0; i < NDSMATRIXSTACK_COUNT(MATRIXMODE_POSITION_VECTOR); i++) { MatrixInit(mtxStackPositionVector[i]); }
	MatrixInit(mtxStackTexture[0]);

	_ML4x4ind = 0;
	_ML4x3ind = 0;
	_MM4x4ind = 0;
	_MM4x3ind = 0;
	_MM3x3ind = 0;

	_BTind = 0;
	_PTind = 0;

	_regTexCoord.s = 0;
	_regTexCoord.t = 0;
	_texCoordTransformed.s = 0;
	_texCoordTransformed.t = 0;
	
	gfx3d.viewport.x = 0;
	gfx3d.viewport.y = 0;
	gfx3d.viewport.width = 256;
	gfx3d.viewport.height = 192;
	
	// Init for save state compatibility
	_GFX3D_IORegisterMap->VIEWPORT.X1 = 0;
	_GFX3D_IORegisterMap->VIEWPORT.Y1 = 0;
	_GFX3D_IORegisterMap->VIEWPORT.X2 = 255;
	_GFX3D_IORegisterMap->VIEWPORT.Y2 = 191;
	gfx3d.viewportLegacySave = _GFX3D_IORegisterMap->VIEWPORT;
	
	isSwapBuffers = FALSE;

	GFX_PIPEclear();
	GFX_FIFOclear();
	
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

static void GEM_TransformVertex(const s32 (&__restrict mtx)[16], s32 (&__restrict vec)[4])
{
	MatrixMultVec4x4(mtx, vec);
}
//---------------


#define SUBMITVERTEX(ii, nn) pendingGList.rawPolyList[pendingGList.rawPolyCount].vertIndexes[ii] = _polyGenInfo.vtxIndex[nn];
//Submit a vertex to the GE
static void SetVertex()
{
	GFX3D_GeometryList &pendingGList = gfx3d.gList[gfx3d.pendingListIndex];

	if (_texCoordTransformMode == TextureTransformationMode_VertexSource)
	{
		//Tested by: Eledees The Adventures of Kai and Zero (E) [title screen and frontend menus]
		const s32 *mtxTex = _mtxCurrent[MATRIXMODE_TEXTURE];
		_texCoordTransformed.s = (s32)( (s64)(GEM_Mul32x16To64(mtxTex[0], _vtxCoord16.x) + GEM_Mul32x16To64(mtxTex[4], _vtxCoord16.y) + GEM_Mul32x16To64(mtxTex[8], _vtxCoord16.z) + ((s64)_regTexCoord.s << 24)) >> 24 );
		_texCoordTransformed.t = (s32)( (s64)(GEM_Mul32x16To64(mtxTex[1], _vtxCoord16.x) + GEM_Mul32x16To64(mtxTex[5], _vtxCoord16.y) + GEM_Mul32x16To64(mtxTex[9], _vtxCoord16.z) + ((s64)_regTexCoord.t << 24)) >> 24 );
	}

	//refuse to do anything if we have too many verts or polys
	_polygonListCompleted = 0;
	if (pendingGList.rawVertCount >= VERTLIST_SIZE)
	{
		return;
	}
	
	if (pendingGList.rawPolyCount >= POLYLIST_SIZE)
	{
		return;
	}
	
	CACHE_ALIGN VertexCoord32x4 coordTransformed = {
		(s32)_vtxCoord16.x,
		(s32)_vtxCoord16.y,
		(s32)_vtxCoord16.z,
		(s32)(1<<12)
	};

	if (freelookMode == 2)
	{
		//adjust projection
		s32 tmp[16];
		MatrixCopy(tmp, _mtxCurrent[MATRIXMODE_PROJECTION]);
		MatrixMultiply(tmp, freelookMatrix);
		GEM_TransformVertex(_mtxCurrent[MATRIXMODE_POSITION], coordTransformed.coord); //modelview
		GEM_TransformVertex(tmp, coordTransformed.coord); //projection
	}
	else if (freelookMode == 3)
	{
		//use provided projection
		GEM_TransformVertex(_mtxCurrent[MATRIXMODE_POSITION], coordTransformed.coord); //modelview
		GEM_TransformVertex(freelookMatrix, coordTransformed.coord); //projection
	}
	else
	{
		//no freelook
		GEM_TransformVertex(_mtxCurrent[MATRIXMODE_POSITION], coordTransformed.coord); //modelview
		GEM_TransformVertex(_mtxCurrent[MATRIXMODE_PROJECTION], coordTransformed.coord); //projection
	}

	//TODO - culling should be done here.
	//TODO - viewport transform?

	size_t continuation = 0;
	if ((_vtxFormat == GFX3D_TRIANGLE_STRIP) && !_polyGenInfo.isFirstPolyCompleted)
		continuation = 2;
	else if ((_vtxFormat == GFX3D_QUAD_STRIP) && !_polyGenInfo.isFirstPolyCompleted)
		continuation = 2;

	//record the vertex
	const size_t vertIndex = pendingGList.rawVertCount + _polyGenInfo.vtxCount - continuation;
	if (vertIndex >= VERTLIST_SIZE)
	{
		printf("wtf\n");
	}
	
	VERT &vert = pendingGList.rawVertList[vertIndex];

	//printf("%f %f %f\n",coordTransformed[0],coordTransformed[1],coordTransformed[2]);
	//if(coordTransformed[1] > 20) 
	//	coordTransformed[1] = 20;

	//printf("y-> %f\n",coord[1]);

	//if(_mtxCurrent[1][14]>15) {
	//	printf("ACK!\n");
	//	printf("----> modelview 1 state for that ack:\n");
	//	//MatrixPrint(_mtxCurrent[1]);
	//}

	vert.texcoord[0] = (float)_texCoordTransformed.s / 16.0f;
	vert.texcoord[1] = (float)_texCoordTransformed.t / 16.0f;
	vert.coord[0] = (float)coordTransformed.x / 4096.0f;
	vert.coord[1] = (float)coordTransformed.y / 4096.0f;
	vert.coord[2] = (float)coordTransformed.z / 4096.0f;
	vert.coord[3] = (float)coordTransformed.w / 4096.0f;
	vert.color[0] = GFX3D_5TO6_LOOKUP(_vtxColor555X.r);
	vert.color[1] = GFX3D_5TO6_LOOKUP(_vtxColor555X.g);
	vert.color[2] = GFX3D_5TO6_LOOKUP(_vtxColor555X.b);
	vert.rf = (float)vert.r;
	vert.gf = (float)vert.g;
	vert.bf = (float)vert.b;
	_polyGenInfo.vtxIndex[_polyGenInfo.vtxCount] = (u16)(pendingGList.rawVertCount + _polyGenInfo.vtxCount - continuation);
	_polyGenInfo.vtxCount++;

	//possibly complete a polygon
	{
		_polygonListCompleted = 2;
		switch(_vtxFormat)
		{
			case GFX3D_TRIANGLES:
			{
				if (_polyGenInfo.vtxCount != 3)
				{
					break;
				}
				
				_polygonListCompleted = 1;
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,2);
				gfx3d.gList[gfx3d.pendingListIndex].rawVertCount += 3;
				pendingGList.rawPolyList[pendingGList.rawPolyCount].type = POLYGON_TYPE_TRIANGLE;
				_polyGenInfo.vtxCount = 0;
				break;
			}
				
			case GFX3D_QUADS:
			{
				if (_polyGenInfo.vtxCount != 4)
				{
					break;
				}
				
				_polygonListCompleted = 1;
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,2);
				SUBMITVERTEX(3,3);
				gfx3d.gList[gfx3d.pendingListIndex].rawVertCount += 4;
				pendingGList.rawPolyList[pendingGList.rawPolyCount].type = POLYGON_TYPE_QUAD;
				_polyGenInfo.vtxCount = 0;
				break;
			}
				
			case GFX3D_TRIANGLE_STRIP:
			{
				if (_polyGenInfo.vtxCount != 3)
				{
					break;
				}
				
				_polygonListCompleted = 1;
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,2);
				pendingGList.rawPolyList[pendingGList.rawPolyCount].type = POLYGON_TYPE_TRIANGLE;
				
				if (_triStripToggle)
				{
					_polyGenInfo.vtxIndex[1] = (u16)(pendingGList.rawVertCount + 2 - continuation);
				}
				else
				{
					_polyGenInfo.vtxIndex[0] = (u16)(pendingGList.rawVertCount + 2 - continuation);
				}
				
				if (_polyGenInfo.isFirstPolyCompleted)
				{
					pendingGList.rawVertCount += 3;
				}
				else
				{
					pendingGList.rawVertCount += 1;
				}
				
				_triStripToggle ^= 1;
				_polyGenInfo.isFirstPolyCompleted = false;
				_polyGenInfo.vtxCount = 2;
				break;
			}
				
			case GFX3D_QUAD_STRIP:
			{
				if (_polyGenInfo.vtxCount != 4)
				{
					break;
				}
				
				_polygonListCompleted = 1;
				SUBMITVERTEX(0,0);
				SUBMITVERTEX(1,1);
				SUBMITVERTEX(2,3);
				SUBMITVERTEX(3,2);
				pendingGList.rawPolyList[pendingGList.rawPolyCount].type = POLYGON_TYPE_QUAD;
				_polyGenInfo.vtxIndex[0] = (u16)(pendingGList.rawVertCount + 2 - continuation);
				_polyGenInfo.vtxIndex[1] = (u16)(pendingGList.rawVertCount + 3 - continuation);
				
				if (_polyGenInfo.isFirstPolyCompleted)
				{
					pendingGList.rawVertCount += 4;
				}
				else
				{
					pendingGList.rawVertCount += 2;
				}
				
				_polyGenInfo.isFirstPolyCompleted = false;
				_polyGenInfo.vtxCount = 2;
				break;
			}
				
			default:
				return;
		}

		if (_polygonListCompleted == 1)
		{
			POLY &poly = pendingGList.rawPolyList[pendingGList.rawPolyCount];
			
			poly.vtxFormat = _vtxFormat;

			// Line segment detect
			// Tested" Castlevania POR - warp stone, trajectory of ricochet, "Eye of Decay"
			if (_currentPolyTexParam.PackedFormat == TEXMODE_NONE)
			{
				bool duplicated = false;
				const VERT &vert0 = pendingGList.rawVertList[poly.vertIndexes[0]];
				const VERT &vert1 = pendingGList.rawVertList[poly.vertIndexes[1]];
				const VERT &vert2 = pendingGList.rawVertList[poly.vertIndexes[2]];
				
				if ( (vert0.x == vert1.x) && (vert0.y == vert1.y) ) duplicated = true;
				else
					if ( (vert1.x == vert2.x) && (vert1.y == vert2.y) ) duplicated = true;
					else
						if ( (vert0.y == vert1.y) && (vert1.y == vert2.y) ) duplicated = true;
						else
							if ( (vert0.x == vert1.x) && (vert1.x == vert2.x) ) duplicated = true;
				if (duplicated)
				{
					//printf("Line Segmet detected (poly type %i, mode %i, texparam %08X)\n", poly.type, poly._vtxFormat, textureFormat);
					poly.vtxFormat = (PolygonPrimitiveType)(_vtxFormat + 4);
				}
			}

			poly.attribute = _polyAttrInProcess;
			poly.texParam = _currentPolyTexParam;
			poly.texPalette = _currentPolyTexPalette;
			poly.viewport = gfx3d.viewport;
			gfx3d.rawPolyViewportLegacySave[pendingGList.rawPolyCount] = _GFX3D_IORegisterMap->VIEWPORT;
			pendingGList.rawPolyCount++;
		}
	}
}

static void UpdateProjection()
{
#ifdef HAVE_LUA
	if(freelookMode == 0) return;
	float floatproj[16];
	for(int i=0;i<16;i++)
		floatproj[i] = _mtxCurrent[MATRIXMODE_PROJECTION][i]/((float)(1<<12));
	CallRegistered3dEvent(0,floatproj);
#endif
}

static void gfx3d_glPolygonAttrib_cache()
{
	// Light enable/disable
	_lightMask = _polyAttrInProcess.LightMask;
}

static void gfx3d_glTexImage_cache()
{
	_texCoordTransformMode = (TextureTransformationMode)_currentPolyTexParam.TexCoordTransformMode;
}

static void gfx3d_glLightDirection_cache(const size_t index)
{
	const u32 v = _regLightDirection[index];
	_cacheLightDirection[index].x = ((s32)((v<<22) & 0xFFC00000) / (s32)(1<<22)) * (s32)(1<<3);
	_cacheLightDirection[index].y = ((s32)((v<<12) & 0xFFC00000) / (s32)(1<<22)) * (s32)(1<<3);
	_cacheLightDirection[index].z = ((s32)((v<< 2) & 0xFFC00000) / (s32)(1<<22)) * (s32)(1<<3);
	_cacheLightDirection[index].w = 0;

	//Multiply the vector by the directional matrix
	MatrixMultVec3x3(_mtxCurrent[MATRIXMODE_POSITION_VECTOR], _cacheLightDirection[index].vec);

	//Calculate the half angle vector
	CACHE_ALIGN const Vector32x4 lineOfSight = {0, 0, (s32)0xFFFFF000, 0};
	_cacheHalfVector[index].x = _cacheLightDirection[index].x + lineOfSight.x;
	_cacheHalfVector[index].y = _cacheLightDirection[index].y + lineOfSight.y;
	_cacheHalfVector[index].z = _cacheLightDirection[index].z + lineOfSight.z;
	_cacheHalfVector[index].w = _cacheLightDirection[index].w + lineOfSight.w;
	
	//normalize the half angle vector
	//can't believe the hardware really does this... but yet it seems...
	s32 halfLength = ((s32)(sqrt((double)vec3dot_fixed32(_cacheHalfVector[index].vec, _cacheHalfVector[index].vec))))<<6;

	if (halfLength != 0)
	{
		halfLength = abs(halfLength);
		halfLength >>= 6;
		for (size_t i = 0; i < 4; i++)
		{
			s32 temp = _cacheHalfVector[index].vec[i];
			temp <<= 6;
			temp /= halfLength;
			_cacheHalfVector[index].vec[i] = temp;
		}
	}
}


//===============================================================================
static void gfx3d_glMatrixMode(const u32 param)
{
	_mtxMode = (MatrixMode)(param & 0x00000003);
	GFX_DELAY(1);
}

static void gfx3d_glPushMatrix()
{
	//1. apply offset specified by push (well, it's always +1) to internal counter
	//2. mask that bit off to actually index the matrix for reading
	//3. SE is set depending on resulting internal counter

	//printf("%d %d %d %d -> ",mtxStack[0].position,mtxStack[1].position,mtxStack[2].position,mtxStack[3].position);
	//printf("PUSH mode: %d -> ",_mtxMode,mtxStack[_mtxMode].position);

	if (_mtxMode == MATRIXMODE_PROJECTION || _mtxMode == MATRIXMODE_TEXTURE)
	{
		if (_mtxMode == MATRIXMODE_PROJECTION)
		{
			MatrixCopy(mtxStackProjection[0], _mtxCurrent[_mtxMode]);
			
			u32 &index = mtxStackIndex[MATRIXMODE_PROJECTION];
			if (index == 1)
			{
				MMU_new.gxstat.se = 1;
			}
			index += 1;
			index &= 1;

			UpdateProjection();
		}
		else
		{
			MatrixCopy(mtxStackTexture[0], _mtxCurrent[_mtxMode]);
			
			u32 &index = mtxStackIndex[MATRIXMODE_TEXTURE];
			if (index == 1)
			{
				MMU_new.gxstat.se = 1; //unknown if this applies to the texture matrix
			}
			index += 1;
			index &= 1;
		}
	}
	else
	{
		u32 &index = mtxStackIndex[MATRIXMODE_POSITION];
		
		MatrixCopy(mtxStackPosition[index & 0x0000001F], _mtxCurrent[MATRIXMODE_POSITION]);
		MatrixCopy(mtxStackPositionVector[index & 0x0000001F], _mtxCurrent[MATRIXMODE_POSITION_VECTOR]);
		
		index += 1;
		index &= 0x0000003F;
		if (index >= 32)
		{
			MMU_new.gxstat.se = 1; //(not sure, this might be off by 1)
		}
	}

	//printf("%d %d %d %d\n",mtxStack[0].position,mtxStack[1].position,mtxStack[2].position,mtxStack[3].position);

	GFX_DELAY(17);
}

static void gfx3d_glPopMatrix(const u32 param)
{
	//1. apply offset specified by pop to internal counter
	//2. SE is set depending on resulting internal counter
	//3. mask that bit off to actually index the matrix for reading

	//printf("%d %d %d %d -> ",mtxStack[0].position,mtxStack[1].position,mtxStack[2].position,mtxStack[3].position);
	//printf("POP   (%d): mode: %d -> ",v,_mtxMode,mtxStack[_mtxMode].position);

	if (_mtxMode == MATRIXMODE_PROJECTION || _mtxMode == MATRIXMODE_TEXTURE)
	{
		//parameter is ignored and treated as sensible (always 1)
		
		if (_mtxMode == MATRIXMODE_PROJECTION)
		{
			u32 &index = mtxStackIndex[MATRIXMODE_PROJECTION];
			index ^= 1;
			if (index == 1)
			{
				MMU_new.gxstat.se = 1;
			}
			MatrixCopy(_mtxCurrent[_mtxMode], mtxStackProjection[0]);

			UpdateProjection();
		}
		else
		{
			u32 &index = mtxStackIndex[MATRIXMODE_TEXTURE];
			index ^= 1;
			if (index == 1)
			{
				MMU_new.gxstat.se = 1; //unknown if this applies to the texture matrix
			}
			MatrixCopy(_mtxCurrent[_mtxMode], mtxStackTexture[0]);
		}
	}
	else
	{
		u32 &i = mtxStackIndex[MATRIXMODE_POSITION];
		
		i -= param & 0x0000003F;
		i &= 0x0000003F;
		if (i >= 32)
		{
			MMU_new.gxstat.se = 1; //(not sure, this might be off by 1)
		}
		
		MatrixCopy(_mtxCurrent[MATRIXMODE_POSITION], mtxStackPosition[i & 0x0000001F]);
		MatrixCopy(_mtxCurrent[MATRIXMODE_POSITION_VECTOR], mtxStackPositionVector[i & 0x0000001F]);
	}

	//printf("%d %d %d %d\n",mtxStack[0].position,mtxStack[1].position,mtxStack[2].position,mtxStack[3].position);

	GFX_DELAY(36);
}

static void gfx3d_glStoreMatrix(const u32 param)
{
	//printf("%d %d %d %d -> ",mtxStack[0].position,mtxStack[1].position,mtxStack[2].position,mtxStack[3].position);
	//printf("STORE (%d): mode: %d -> ",v,_mtxMode,mtxStack[_mtxMode].position);

	if (_mtxMode == MATRIXMODE_PROJECTION || _mtxMode == MATRIXMODE_TEXTURE)
	{
		//parameter ignored and treated as sensible
		if (_mtxMode == MATRIXMODE_PROJECTION)
		{
			MatrixCopy(mtxStackProjection[0], _mtxCurrent[MATRIXMODE_PROJECTION]);
			UpdateProjection();
		}
		else
		{
			MatrixCopy(mtxStackTexture[0], _mtxCurrent[MATRIXMODE_TEXTURE]);
		}
	}
	else
	{
		//out of bounds function fully properly, but set errors (not sure, this might be off by 1)
		if (param >= 31)
		{
			MMU_new.gxstat.se = 1;
		}
		
		const u32 i = param & 0x0000001F;
		MatrixCopy(mtxStackPosition[i], _mtxCurrent[MATRIXMODE_POSITION]);
		MatrixCopy(mtxStackPositionVector[i], _mtxCurrent[MATRIXMODE_POSITION_VECTOR]);
	}

	//printf("%d %d %d %d\n",mtxStack[0].position,mtxStack[1].position,mtxStack[2].position,mtxStack[3].position);

	GFX_DELAY(17);
}

static void gfx3d_glRestoreMatrix(const u32 param)
{
	if (_mtxMode == MATRIXMODE_PROJECTION || _mtxMode == MATRIXMODE_TEXTURE)
	{
		//parameter ignored and treated as sensible
		if (_mtxMode == MATRIXMODE_PROJECTION)
		{
			MatrixCopy(_mtxCurrent[MATRIXMODE_PROJECTION], mtxStackProjection[0]);
			UpdateProjection();
		}
		else
		{
			MatrixCopy(_mtxCurrent[MATRIXMODE_TEXTURE], mtxStackTexture[0]);
		}
	}
	else
	{
		//out of bounds errors function fully properly, but set errors
		MMU_new.gxstat.se = (param >= 31) ? 1 : 0; //(not sure, this might be off by 1)
		
		const u32 i = param & 0x0000001F;
		MatrixCopy(_mtxCurrent[MATRIXMODE_POSITION], mtxStackPosition[i]);
		MatrixCopy(_mtxCurrent[MATRIXMODE_POSITION_VECTOR], mtxStackPositionVector[i]);
	}

	GFX_DELAY(36);
}

static void gfx3d_glLoadIdentity()
{
	MatrixIdentity(_mtxCurrent[_mtxMode]);
	GFX_DELAY(19);

	if (_mtxMode == MATRIXMODE_POSITION_VECTOR)
		MatrixIdentity(_mtxCurrent[MATRIXMODE_POSITION]);

	//printf("identity: %d to: \n",_mtxMode); MatrixPrint(_mtxCurrent[1]);
}

static void gfx3d_glLoadMatrix4x4(const u32 param)
{
	_mtxCurrent[_mtxMode][_ML4x4ind] = (s32)param;
	_ML4x4ind++;
	
	if (_ML4x4ind < 16)
	{
		return;
	}
	_ML4x4ind = 0;

	GFX_DELAY(19);

	if (_mtxMode == MATRIXMODE_POSITION_VECTOR)
		MatrixCopy(_mtxCurrent[MATRIXMODE_POSITION], _mtxCurrent[MATRIXMODE_POSITION_VECTOR]);

	//printf("load4x4: matrix %d to: \n",_mtxMode); MatrixPrint(_mtxCurrent[1]);
}

static void gfx3d_glLoadMatrix4x3(const u32 param)
{
	_mtxCurrent[_mtxMode][_ML4x3ind] = (s32)param;
	_ML4x3ind++;
	
	if ((_ML4x3ind & 0x03) == 3)
	{
		_ML4x3ind++;
	}
	
	if (_ML4x3ind < 16)
	{
		return;
	}
	_ML4x3ind = 0;

	//fill in the unusued matrix values
	_mtxCurrent[_mtxMode][3] = _mtxCurrent[_mtxMode][7] = _mtxCurrent[_mtxMode][11] = 0;
	_mtxCurrent[_mtxMode][15] = (s32)(1 << 12);

	GFX_DELAY(30);

	if (_mtxMode == MATRIXMODE_POSITION_VECTOR)
		MatrixCopy(_mtxCurrent[MATRIXMODE_POSITION], _mtxCurrent[MATRIXMODE_POSITION_VECTOR]);
	//printf("load4x3: matrix %d to: \n",_mtxMode); MatrixPrint(_mtxCurrent[1]);
}

static void gfx3d_glMultMatrix4x4(const u32 param)
{
	_mtxTemporal[_MM4x4ind] = (s32)param;
	_MM4x4ind++;
	
	if (_MM4x4ind < 16)
	{
		return;
	}
	_MM4x4ind = 0;

	MatrixMultiply(_mtxCurrent[_mtxMode], _mtxTemporal);
	GFX_DELAY(35);

	if (_mtxMode == MATRIXMODE_POSITION_VECTOR)
	{
		MatrixMultiply(_mtxCurrent[MATRIXMODE_POSITION], _mtxTemporal);
		GFX_DELAY_M2(30);
	}

	if (_mtxMode == MATRIXMODE_PROJECTION)
	{
		UpdateProjection();
	}

	//printf("mult4x4: matrix %d to: \n",_mtxMode); MatrixPrint(_mtxCurrent[1]);

	MatrixIdentity(_mtxTemporal);
}

static void gfx3d_glMultMatrix4x3(const u32 param)
{
	_mtxTemporal[_MM4x3ind] = (s32)param;
	_MM4x3ind++;
	
	if ((_MM4x3ind & 0x03) == 3)
	{
		_MM4x3ind++;
	}
	
	if (_MM4x3ind < 16)
	{
		return;
	}
	_MM4x3ind = 0;

	//fill in the unusued matrix values
	_mtxTemporal[3] = _mtxTemporal[7] = _mtxTemporal[11] = 0;
	_mtxTemporal[15] = (s32)(1 << 12);

	MatrixMultiply(_mtxCurrent[_mtxMode], _mtxTemporal);
	GFX_DELAY(31);

	if (_mtxMode == MATRIXMODE_POSITION_VECTOR)
	{
		MatrixMultiply(_mtxCurrent[MATRIXMODE_POSITION], _mtxTemporal);
		GFX_DELAY_M2(30);
	}

	if (_mtxMode == MATRIXMODE_PROJECTION)
	{
		UpdateProjection();
	}

	//printf("mult4x3: matrix %d to: \n",_mtxMode); MatrixPrint(_mtxCurrent[1]);

	//does this really need to be done?
	MatrixIdentity(_mtxTemporal);
}

static void gfx3d_glMultMatrix3x3(const u32 param)
{
	_mtxTemporal[_MM3x3ind] = (s32)param;
	_MM3x3ind++;
	
	if ((_MM3x3ind & 0x03) == 3)
	{
		_MM3x3ind++;
	}
	
	if (_MM3x3ind < 12)
	{
		return;
	}
	_MM3x3ind = 0;

	//fill in the unusued matrix values
	_mtxTemporal[3] = _mtxTemporal[7] = _mtxTemporal[11] = 0;
	_mtxTemporal[15] = 1<<12;
	_mtxTemporal[12] = _mtxTemporal[13] = _mtxTemporal[14] = 0;

	MatrixMultiply(_mtxCurrent[_mtxMode], _mtxTemporal);
	GFX_DELAY(28);

	if (_mtxMode == MATRIXMODE_POSITION_VECTOR)
	{
		MatrixMultiply(_mtxCurrent[MATRIXMODE_POSITION], _mtxTemporal);
		GFX_DELAY_M2(30);
	}

	if (_mtxMode == MATRIXMODE_PROJECTION)
	{
		UpdateProjection();
	}

	//printf("mult3x3: matrix %d to: \n",_mtxMode); MatrixPrint(_mtxCurrent[1]);


	//does this really need to be done?
	MatrixIdentity(_mtxTemporal);
}

static void gfx3d_glScale(const u32 param)
{
	_regScale.vec[_regScaleCurrentIndex] = (s32)param;
	_regScaleCurrentIndex++;

	if (_regScaleCurrentIndex < 3)
	{
		return;
	}
	_regScaleCurrentIndex = 0;

	MatrixScale(_mtxCurrent[(_mtxMode == MATRIXMODE_POSITION_VECTOR ? MATRIXMODE_POSITION : _mtxMode)], _regScale.vec);
	GFX_DELAY(22);
	//printf("scale: matrix %d to: \n",_mtxMode); MatrixPrint(_mtxCurrent[1]);

	//note: pos-vector mode should not cause both matrices to scale.
	//the whole purpose is to keep the vector matrix orthogonal
	//so, I am leaving this commented out as an example of what not to do.
	//if (_mtxMode == MATRIXMODE_POSITION_VECTOR)
	//	MatrixScale (_mtxCurrent[MATRIXMODE_POSITION], _regScale.vec);
}

static void gfx3d_glTranslate(const u32 param)
{
	_regTranslate.vec[_regTranslateCurrentIndex] = (s32)param;
	_regTranslateCurrentIndex++;

	if (_regTranslateCurrentIndex < 3)
	{
		return;
	}
	_regTranslateCurrentIndex = 0;

	MatrixTranslate(_mtxCurrent[_mtxMode], _regTranslate.vec);
	GFX_DELAY(22);

	if (_mtxMode == MATRIXMODE_POSITION_VECTOR)
	{
		MatrixTranslate(_mtxCurrent[MATRIXMODE_POSITION], _regTranslate.vec);
		GFX_DELAY_M2(30);
	}

	//printf("translate: matrix %d to: \n",_mtxMode); MatrixPrint(_mtxCurrent[1]);
}

static void gfx3d_glColor3b(const u32 param)
{
	_vtxColor555X.r = (u8)((param >>  0) & 0x0000001F);
	_vtxColor555X.g = (u8)((param >>  5) & 0x0000001F);
	_vtxColor555X.b = (u8)((param >> 10) & 0x0000001F);
	
	GFX_DELAY(1);
}

static void gfx3d_glNormal(const u32 param)
{
	CACHE_ALIGN Vector32x4 normal = {
		((s32)((param << 22) & 0xFFC00000) / (s32)(1<<22)) * (s32)(1<<3),
		((s32)((param << 12) & 0xFFC00000) / (s32)(1<<22)) * (s32)(1<<3),
		((s32)((param <<  2) & 0xFFC00000) / (s32)(1<<22)) * (s32)(1<<3),
		 (s32)(1 << 12)
	};

	if (_texCoordTransformMode == TextureTransformationMode_NormalSource)
	{
		//SM64 highlight rendered star in main menu tests this
		//also smackdown 2010 player textures tested this (needed cast on _s and _t)
		const s32 *mtxTex = _mtxCurrent[MATRIXMODE_TEXTURE];
		_texCoordTransformed.s = (s32)( (s64)(GEM_Mul32x32To64(mtxTex[0], normal.x) + GEM_Mul32x32To64(mtxTex[4], normal.y) + GEM_Mul32x32To64(mtxTex[8], normal.z) + ((s64)_regTexCoord.s << 24)) >> 24 );
		_texCoordTransformed.t = (s32)( (s64)(GEM_Mul32x32To64(mtxTex[1], normal.x) + GEM_Mul32x32To64(mtxTex[5], normal.y) + GEM_Mul32x32To64(mtxTex[9], normal.z) + ((s64)_regTexCoord.t << 24)) >> 24 );
	}

	MatrixMultVec3x3(_mtxCurrent[MATRIXMODE_POSITION_VECTOR], normal.vec);

	//apply lighting model
	const s32 diffuse32[3] = {
		(s32)( _regDiffuse        & 0x001F),
		(s32)((_regDiffuse >>  5) & 0x001F),
		(s32)((_regDiffuse >> 10) & 0x001F)
	};

	const s32 ambient32[3] = {
		(s32)( _regAmbient        & 0x001F),
		(s32)((_regAmbient >>  5) & 0x001F),
		(s32)((_regAmbient >> 10) & 0x001F)
	};

	const s32 emission32[3] = {
		(s32)( _regEmission        & 0x001F),
		(s32)((_regEmission >>  5) & 0x001F),
		(s32)((_regEmission >> 10) & 0x001F)
	};

	const s32 specular32[3] = {
		(s32)( _regSpecular        & 0x001F),
		(s32)((_regSpecular >>  5) & 0x001F),
		(s32)((_regSpecular >> 10) & 0x001F)
	};

	s32 vertexColor[3] = {
		emission32[0],
		emission32[1],
		emission32[2]
	};

	for (size_t i = 0; i < 4; i++)
	{
		if (!((_lightMask >> i) & 1))
		{
			continue;
		}
		
		const s32 lightColor32[3] = {
			(s32)( _regLightColor[i]        & 0x0000001F),
			(s32)((_regLightColor[i] >>  5) & 0x0000001F),
			(s32)((_regLightColor[i] >> 10) & 0x0000001F)
		};

		//This formula is the one used by the DS
		//Reference : http://nocash.emubase.de/gbatek.htm#ds3dpolygonlightparameters
		const s32 fixed_diffuse = std::max( 0, -vec3dot_fixed32(_cacheLightDirection[i].vec, normal.vec) );
		
		//todo - this could be cached in this form
		const Vector32x4 fixedTempNegativeHalf = {
			-_cacheHalfVector[i].x,
			-_cacheHalfVector[i].y,
			-_cacheHalfVector[i].z,
			-_cacheHalfVector[i].w
		};
		const s32 dot = vec3dot_fixed32(fixedTempNegativeHalf.vec, normal.vec);

		s32 fixedshininess = 0;
		if (dot > 0) //prevent shininess on opposite side
		{
			//we have cos(a). it seems that we need cos(2a). trig identity is a fast way to get it.
			//cos^2(a)=(1/2)(1+cos(2a))
			//2*cos^2(a)-1=cos(2a)
			fixedshininess = 2 * mul_fixed32(dot,dot) - 4096;
			//gbatek is almost right but not quite!
		}

		//this seems to need to be saturated, or else the table will overflow.
		//even without a table, failure to saturate is bad news
		fixedshininess = std::min(fixedshininess,4095);
		fixedshininess = std::max(fixedshininess,0);
		
		if (_regSpecular & 0x8000)
		{
			//shininess is 20.12 fixed point, so >>5 gives us .7 which is 128 entries
			//the entries are 8bits each so <<4 gives us .12 again, compatible with the lighting formulas below
			//(according to other normal nds procedures, we might should fill the bottom bits with 1 or 0 according to rules...)
			fixedshininess = gfx3d.pendingState.shininessTable[fixedshininess>>5]<<4;
		}

		for (size_t c = 0; c < 3; c++)
		{
			const s32 specComp = ((specular32[c] * lightColor32[c] * fixedshininess) >> 17); // 5 bits for color*color and 12 bits for shininess
			const s32 diffComp = (( diffuse32[c] * lightColor32[c] * fixed_diffuse)  >> 17); // 5 bits for color*color and 12 bits for diffuse
			const s32 ambComp  = (( ambient32[c] * lightColor32[c]) >> 5); // 5 bits for color*color
			vertexColor[c] += specComp + diffComp + ambComp;
		}
	}

	_vtxColor555X.r = (u8)std::min<s32>(31, vertexColor[0]);
	_vtxColor555X.g = (u8)std::min<s32>(31, vertexColor[1]);
	_vtxColor555X.b = (u8)std::min<s32>(31, vertexColor[2]);

	GFX_DELAY(9);
	GFX_DELAY_M2((_lightMask) & 0x01);
	GFX_DELAY_M2((_lightMask>>1) & 0x01);
	GFX_DELAY_M2((_lightMask>>2) & 0x01);
	GFX_DELAY_M2((_lightMask>>3) & 0x01);
}

static void gfx3d_glTexCoord(const u32 param)
{
	VertexCoord16x2 inTexCoord16x2;
	inTexCoord16x2.value = param;
	
	_regTexCoord.s = (s32)inTexCoord16x2.s;
	_regTexCoord.t = (s32)inTexCoord16x2.t;

	if (_texCoordTransformMode == TextureTransformationMode_TexCoordSource)
	{
		//dragon quest 4 overworld will test this
		const s32 *mtxTex = _mtxCurrent[MATRIXMODE_TEXTURE];
		_texCoordTransformed.s = (s32)( (s64)(GEM_Mul32x32To64(mtxTex[0], _regTexCoord.s) + GEM_Mul32x32To64(mtxTex[4], _regTexCoord.t) + (s64)mtxTex[8] + (s64)mtxTex[12]) >> 12 );
		_texCoordTransformed.t = (s32)( (s64)(GEM_Mul32x32To64(mtxTex[1], _regTexCoord.s) + GEM_Mul32x32To64(mtxTex[5], _regTexCoord.t) + (s64)mtxTex[9] + (s64)mtxTex[13]) >> 12 );
	}
	else if (_texCoordTransformMode == TextureTransformationMode_None)
	{
		_texCoordTransformed.s = _regTexCoord.s;
		_texCoordTransformed.t = _regTexCoord.t;
	}
	
	GFX_DELAY(1);
}

static void gfx3d_glVertex16b(const u32 param)
{
	VertexCoord16x2 inVtx16x2;
	inVtx16x2.value = param;
	
	if (_vtxCoord16CurrentIndex == 0)
	{
		_vtxCoord16.coord[0] = (s32)inVtx16x2.coord[0];
		_vtxCoord16.coord[1] = (s32)inVtx16x2.coord[1];
		_vtxCoord16CurrentIndex++;
		return;
	}

	_vtxCoord16.coord[2] = (s32)inVtx16x2.coord[0];
	_vtxCoord16CurrentIndex = 0;
	
	SetVertex();
	GFX_DELAY(9);
}

static void gfx3d_glVertex10b(const u32 param)
{
	_vtxCoord16.x = (s16)( (u16)(((param << 22) >> 22) << 6) );
	_vtxCoord16.y = (s16)( (u16)(((param << 12) >> 22) << 6) );
	_vtxCoord16.z = (s16)( (u16)(((param <<  2) >> 22) << 6) );
	
	SetVertex();
	GFX_DELAY(8);
}

template<int ONE, int TWO>
static void gfx3d_glVertex3_cord(const u32 param)
{
	VertexCoord16x2 inVtx16x2;
	inVtx16x2.value = param;
	
	_vtxCoord16.coord[ONE] = (s32)inVtx16x2.coord[0];
	_vtxCoord16.coord[TWO] = (s32)inVtx16x2.coord[1];

	SetVertex();
	GFX_DELAY(8);
}

static void gfx3d_glVertex_rel(const u32 param)
{
	const s16 x = (s16)( (s32)((param << 22) & 0xFFC00000) / (s32)(1 << 22) );
	const s16 y = (s16)( (s32)((param << 12) & 0xFFC00000) / (s32)(1 << 22) );
	const s16 z = (s16)( (s32)((param <<  2) & 0xFFC00000) / (s32)(1 << 22) );
	
	_vtxCoord16.coord[0] += x;
	_vtxCoord16.coord[1] += y;
	_vtxCoord16.coord[2] += z;

	SetVertex();
	GFX_DELAY(8);
}

static void gfx3d_glPolygonAttrib(const u32 param)
{
	if (_inBegin)
	{
		//PROGINFO("Set polyattr in the middle of a begin/end pair.\n  (This won't be activated until the next begin)\n");
		//TODO - we need some some similar checking for teximageparam etc.
	}
	
	_currentPolyAttr.value = param;
	GFX_DELAY(1);
}

static void gfx3d_glTexImage(const u32 param)
{
	_currentPolyTexParam.value = param;
	gfx3d_glTexImage_cache();
	GFX_DELAY(1);
}

static void gfx3d_glTexPalette(const u32 param)
{
	_currentPolyTexPalette = param;
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
static void gfx3d_glMaterial0(const u32 param)
{
	_regDiffuse = (u16)(param & 0x0000FFFF);
	_regAmbient = (u16)(param >> 16);

	if (BIT15(param))
	{
		_vtxColor555X.r = (u8)((param >>  0) & 0x0000001F);
		_vtxColor555X.g = (u8)((param >>  5) & 0x0000001F);
		_vtxColor555X.b = (u8)((param >> 10) & 0x0000001F);
	}
	GFX_DELAY(4);
}

static void gfx3d_glMaterial1(const u32 param)
{
	_regSpecular = (u16)(param & 0x0000FFFF);
	_regEmission = (u16)(param >> 16);
	GFX_DELAY(4);
}


// 0-9   Directional Vector's X component (1bit sign + 9bit fractional part)
// 10-19 Directional Vector's Y component (1bit sign + 9bit fractional part)
// 20-29 Directional Vector's Z component (1bit sign + 9bit fractional part)
// 30-31 Light Number                     (0..3)
static void gfx3d_glLightDirection(const u32 param)
{
	const size_t index = param >> 30;

	_regLightDirection[index] = param & 0x3FFFFFFF;
	gfx3d_glLightDirection_cache(index);
	GFX_DELAY(6);
}

static void gfx3d_glLightColor(const u32 param)
{
	const size_t index = param >> 30;
	_regLightColor[index] = param;
	GFX_DELAY(1);
}

static void gfx3d_glShininess(const u32 param)
{
#ifdef MSB_FIRST
	u8 *targetShininess = (u8 *)gfx3d.pendingState.shininessTable;
	targetShininess[_shininessTableCurrentIndex+0] =   (u8)(param        & 0x000000FF);
	targetShininess[_shininessTableCurrentIndex+1] = (u8)(((param >>  8) & 0x000000FF));
	targetShininess[_shininessTableCurrentIndex+2] = (u8)(((param >> 16) & 0x000000FF));
	targetShininess[_shininessTableCurrentIndex+3] = (u8)(((param >> 24) & 0x000000FF));
#else
	u32 &targetShininess = (u32 &)gfx3d.pendingState.shininessTable[_shininessTableCurrentIndex];
	targetShininess = param;
#endif
	
	_shininessTableCurrentIndex += 4;
	
	if (_shininessTableCurrentIndex < 128)
	{
		return;
	}
	_shininessTableCurrentIndex = 0;
	GFX_DELAY(32);
}

static void gfx3d_glBegin(const u32 param)
{
	_inBegin = true;
	_vtxFormat = (PolygonPrimitiveType)(param & 0x00000003);
	_triStripToggle = 0;
	_polyGenInfo.vtxCount = 0;
	_polyGenInfo.isFirstPolyCompleted = true;
	_polyAttrInProcess = _currentPolyAttr;
	gfx3d_glPolygonAttrib_cache();
	GFX_DELAY(1);
}

static void gfx3d_glEnd()
{
	_polyGenInfo.vtxCount = 0;
	_inBegin = false;
	GFX_DELAY(1);
}

// swap buffers - skipped

static void gfx3d_glViewport(const u32 param)
{
	_GFX3D_IORegisterMap->VIEWPORT.value = param;
	gfx3d.viewportLegacySave = _GFX3D_IORegisterMap->VIEWPORT;
	gfx3d.viewport = GFX3D_ViewportParse(param);
	
	GFX_DELAY(1);
}

static void gfx3d_glBoxTest(const u32 param)
{
	//printf("boxtest\n");

	//clear result flag. busy flag has been set by fifo component already
	MMU_new.gxstat.tr = 0;		

	_BTcoords[_BTind++] = (u16)(param & 0x0000FFFF);
	_BTcoords[_BTind++] = (u16)(param >> 16);

	if (_BTind < 5)
	{
		return;
	}
	_BTind = 0;

	GFX_DELAY(103);

	//now that we're executing this, we're not busy anymore
	MMU_new.gxstat.tb = 0;

#if 0
	INFO("BoxTEST: x %f y %f width %f height %f depth %f\n", 
				_BTcoords[0], _BTcoords[1], _BTcoords[2], _BTcoords[3], _BTcoords[4], _BTcoords[5]);
	/*for (int i = 0; i < 16; i++)
	{
		INFO("mtx1[%i] = %f ", i, _mtxCurrent[1][i]);
		if ((i+1) % 4 == 0) INFO("\n");
	}
	INFO("\n");*/
#endif

	//(crafted to be clear, not fast.)

	//nanostray title, ff4, ice age 3 depend on this and work
	//garfields nightmare and strawberry shortcake DO DEPEND on the overflow behavior.

	const u16 ux = _BTcoords[0];
	const u16 uy = _BTcoords[1];
	const u16 uz = _BTcoords[2];
	const u16 uw = _BTcoords[3];
	const u16 uh = _BTcoords[4];
	const u16 ud = _BTcoords[5];

	//craft the coords by adding extents to startpoint
	const s32 fixedOne = 1 << 12;
	const s32 __x = (s32)((s16)ux);
	const s32 __y = (s32)((s16)uy);
	const s32 __z = (s32)((s16)uz);
	const s32 x_w = (s32)( (s16)((ux+uw) & 0xFFFF) );
	const s32 y_h = (s32)( (s16)((uy+uh) & 0xFFFF) );
	const s32 z_d = (s32)( (s16)((uz+ud) & 0xFFFF) );
	
	//eight corners of cube
	CACHE_ALIGN VertexCoord32x4 vtxPosition[8] = {
		{ __x, __y, __z, fixedOne },
		{ x_w, __y, __z, fixedOne },
		{ x_w, y_h, __z, fixedOne },
		{ __x, y_h, __z, fixedOne },
		{ __x, __y, z_d, fixedOne },
		{ x_w, __y, z_d, fixedOne },
		{ x_w, __y, z_d, fixedOne },
		{ __x, y_h, z_d, fixedOne }
	};
	
#define SET_VERT_INDICES(p,  a,b,c,d) \
	tempRawPoly[p].type = POLYGON_TYPE_QUAD; \
	tempRawPoly[p].vertIndexes[0] = a; \
	tempRawPoly[p].vertIndexes[1] = b; \
	tempRawPoly[p].vertIndexes[2] = c; \
	tempRawPoly[p].vertIndexes[3] = d;
	
	//craft the faces of the box (clockwise)
	POLY tempRawPoly[6];
	SET_VERT_INDICES(0,  7,6,5,4) // near
	SET_VERT_INDICES(1,  0,1,2,3) // far
	SET_VERT_INDICES(2,  0,3,7,4) // left
	SET_VERT_INDICES(3,  6,2,1,5) // right
	SET_VERT_INDICES(4,  3,2,6,7) // top
	SET_VERT_INDICES(5,  0,4,5,1) // bottom
#undef SET_VERT_INDICES
	
	//setup the clipper
	CPoly tempClippedPoly;

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
	
	// TODO: Remove these floating-point vertices.
	// The internal vertices for the box test are calculated using 20.12 fixed-point, but
	// clipping and interpolation are still calculated in floating-point. We really need
	// to rework the entire clipping and interpolation system to work in fixed-point also.
	CACHE_ALIGN VERT verts[8];

	//transform all coords
	for (size_t i = 0; i < 8; i++)
	{
		MatrixMultVec4x4(_mtxCurrent[MATRIXMODE_POSITION], vtxPosition[i].coord);
		MatrixMultVec4x4(_mtxCurrent[MATRIXMODE_PROJECTION], vtxPosition[i].coord);
		
		// TODO: Remove this fixed-point to floating-point conversion.
		verts[i].x = (float)(vtxPosition[i].x) / 4096.0f;
		verts[i].y = (float)(vtxPosition[i].y) / 4096.0f;
		verts[i].z = (float)(vtxPosition[i].z) / 4096.0f;
		verts[i].w = (float)(vtxPosition[i].w) / 4096.0f;
	}

	//clip each poly
	for (size_t i = 0; i < 6; i++)
	{
		const POLY &rawPoly = tempRawPoly[i];
		const VERT *vertTable[4] = {
			&verts[rawPoly.vertIndexes[0]],
			&verts[rawPoly.vertIndexes[1]],
			&verts[rawPoly.vertIndexes[2]],
			&verts[rawPoly.vertIndexes[3]]
		};

		const PolygonType cpType = GFX3D_GenerateClippedPoly<ClipperMode_DetermineClipOnly>(0, rawPoly.type, vertTable, tempClippedPoly);
		
		//if any portion of this poly was retained, then the test passes.
		if (cpType != POLYGON_TYPE_UNDEFINED)
		{
			//printf("%06d PASS %d\n",gxFIFO.size, i);
			MMU_new.gxstat.tr = 1;
			break;
		}

		//if(i==5) printf("%06d FAIL\n",gxFIFO.size);
	}

	//printf("%06d RESULT %d\n",gxFIFO.size, MMU_new.gxstat.tr);
}

static void gfx3d_glPosTest(const u32 param)
{
	//this is apparently tested by transformers decepticons and ultimate spiderman
	
	// "Apollo Justice: Ace Attorney" also uses the position test for the evidence
	// viewer, such as the card color check in Case 1.

	//clear result flag. busy flag has been set by fifo component already
	MMU_new.gxstat.tr = 0;

	//now that we're executing this, we're not busy anymore
	MMU_new.gxstat.tb = 0;

	// Values for the position test come in as a pair of 16-bit coordinates
	// in 4.12 fixed-point format:
	//     1-bit sign, 3-bit integer, 12-bit fraction
	//
	// Parameter 1, bits  0-15: X-component
	// Parameter 1, bits 16-31: Y-component
	// Parameter 2, bits  0-15: Z-component
	// Parameter 2, bits 16-31: Ignored
	
	// Convert the coordinates to 20.12 fixed-point format for our vector-matrix multiplication.
	_PTcoords.coord[_PTind++] = (s32)( (s16)((u16)(param & 0x0000FFFF)) );
	_PTcoords.coord[_PTind++] = (s32)( (s16)((u16)(param >> 16)) );

	if (_PTind < 3)
	{
		return;
	}
	_PTind = 0;
	
	_PTcoords.coord[3] = (s32)(1 << 12);
	MatrixMultVec4x4(_mtxCurrent[MATRIXMODE_POSITION], _PTcoords.coord);
	MatrixMultVec4x4(_mtxCurrent[MATRIXMODE_PROJECTION], _PTcoords.coord);
	GFX_DELAY(9);

	MMU_new.gxstat.tb = 0;
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x620, (u32)_PTcoords.coord[0]);
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x624, (u32)_PTcoords.coord[1]);
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x628, (u32)_PTcoords.coord[2]);
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x62C, (u32)_PTcoords.coord[3]);
}

static void gfx3d_glVecTest(const u32 param)
{
	//printf("vectest\n");
	GFX_DELAY(5);

	//this is tested by phoenix wright in its evidence inspector modelviewer
	//i am not sure exactly what it is doing, maybe it is testing to ensure
	//that the normal vector for the point of interest is camera-facing.
	
	// Values for the vector test come in as a trio of 10-bit coordinates
	// in 1.9 fixed-point format:
	//     1-bit sign, 9-bit fraction
	//
	// Bits  0- 9: X-component
	// Bits 10-19: Y-component
	// Bits 20-29: Z-component
	// Bits 30-31: Ignored
	
	// Convert the coordinates to 20.12 fixed-point format for our vector-matrix multiplication.
	CACHE_ALIGN Vector32x4 normal = {
		( (s32)((param << 22) & 0xFFC00000) / (s32)(1 << 19) ) | (s32)((param & 0x000001C0) >>  6),
		( (s32)((param << 12) & 0xFFC00000) / (s32)(1 << 19) ) | (s32)((param & 0x00007000) >> 16),
		( (s32)((param <<  2) & 0xFFC00000) / (s32)(1 << 19) ) | (s32)((param & 0x01C00000) >> 26),
		0
	};
	
	MatrixMultVec4x4(_mtxCurrent[MATRIXMODE_POSITION_VECTOR], normal.vec);

	const u16 x = (u16)((s16)normal.x);
	const u16 y = (u16)((s16)normal.y);
	const u16 z = (u16)((s16)normal.z);

	MMU_new.gxstat.tb = 0;		// clear busy
	T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x630, x);
	T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x632, y);
	T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x634, z);

}
//================================================================================= Geometry Engine
//================================================================================= (end)
//=================================================================================
void gfx3d_glFogColor(const u32 v)
{
	gfx3d.pendingState.fogColor = v;
}

void gfx3d_glFogOffset(const u32 v)
{
	gfx3d.pendingState.fogOffset = (u16)(v & 0x00007FFF);
}

template <typename T, size_t ADDROFFSET>
void gfx3d_glClearDepth(const T v)
{
	const size_t numBytes = sizeof(T);

	switch (numBytes)
	{
		case 1:
		{
			if (ADDROFFSET == 1)
			{
				gfx3d.pendingState.clearDepth = DS_DEPTH15TO24(_GFX3D_IORegisterMap->CLEAR_DEPTH);
			}
			break;
		}
			
		case 2:
			gfx3d.pendingState.clearDepth = DS_DEPTH15TO24(v);
			break;
			
		default:
			break;
	}
}

template void gfx3d_glClearDepth< u8, 0>(const  u8 v);
template void gfx3d_glClearDepth< u8, 1>(const  u8 v);
template void gfx3d_glClearDepth<u16, 0>(const u16 v);

template <typename T, size_t ADDROFFSET>
void gfx3d_glClearImageOffset(const T v)
{
	((T *)&gfx3d.pendingState.clearImageOffset)[ADDROFFSET >> (sizeof(T) >> 1)] = v;
}

template void gfx3d_glClearImageOffset< u8, 0>(const  u8 v);
template void gfx3d_glClearImageOffset< u8, 1>(const  u8 v);
template void gfx3d_glClearImageOffset<u16, 0>(const u16 v);

u32 gfx3d_GetNumPolys()
{
	//so is this in the currently-displayed or currently-built list?
	return (u32)(gfx3d.gList[gfx3d.pendingListIndex].rawPolyCount);
}

u32 gfx3d_GetNumVertex()
{
	//so is this in the currently-displayed or currently-built list?
	return (u32)gfx3d.gList[gfx3d.pendingListIndex].rawVertCount;
}

template <typename T>
void gfx3d_UpdateEdgeMarkColorTable(const u8 offset, const T v)
{
	((T *)(gfx3d.pendingState.edgeMarkColorTable))[offset >> (sizeof(T) >> 1)] = v;
}

template void gfx3d_UpdateEdgeMarkColorTable< u8>(const u8 offset, const  u8 v);
template void gfx3d_UpdateEdgeMarkColorTable<u16>(const u8 offset, const u16 v);
template void gfx3d_UpdateEdgeMarkColorTable<u32>(const u8 offset, const u32 v);

template <typename T>
void gfx3d_UpdateFogTable(const u8 offset, const T v)
{
	((T *)(gfx3d.pendingState.fogDensityTable))[offset >> (sizeof(T) >> 1)] = v;
}

template void gfx3d_UpdateFogTable< u8>(const u8 offset, const  u8 v);
template void gfx3d_UpdateFogTable<u16>(const u8 offset, const u16 v);
template void gfx3d_UpdateFogTable<u32>(const u8 offset, const u32 v);

template <typename T>
void gfx3d_UpdateToonTable(const u8 offset, const T v)
{
	//C.O.P. sets toon table via this method
	((T *)(gfx3d.pendingState.toonTable16))[offset >> (sizeof(T) >> 1)] = v;
}

template void gfx3d_UpdateToonTable< u8>(const u8 offset, const  u8 v);
template void gfx3d_UpdateToonTable<u16>(const u8 offset, const u16 v);
template void gfx3d_UpdateToonTable<u32>(const u8 offset, const u32 v);

s32 gfx3d_GetClipMatrix(const u32 index)
{
	//printf("reading clip matrix: %d\n",index);
	return MatrixGetMultipliedIndex(index, _mtxCurrent[MATRIXMODE_PROJECTION], _mtxCurrent[MATRIXMODE_POSITION]);
}

s32 gfx3d_GetDirectionalMatrix(const u32 index)
{
	const size_t _index = (((index / 3) * 4) + (index % 3));

	//return (s32)(_mtxCurrent[2][_index]*(1<<12));
	return _mtxCurrent[MATRIXMODE_POSITION_VECTOR][_index];
}

void gfx3d_glAlphaFunc(const u32 v)
{
	gfx3d.pendingState.alphaTestRef = (u8)(v & 0x0000001F);
}

u32 gfx3d_glGetPosRes(const size_t index)
{
	return (u32)_PTcoords.coord[index];
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
			gfx3d_glViewport(param);
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

void gfx3d_glFlush(const u32 param)
{
	//printf("-------------FLUSH------------- (vcount=%d\n",nds.VCount);
	gfx3d.pendingState.SWAP_BUFFERS.value = param;
#if 0
	if (isSwapBuffers)
	{
		INFO("Error: swapBuffers already use\n");
	}
#endif
	
	isSwapBuffers = TRUE;

	GFX_DELAY(1);
}

static bool gfx3d_ysort_compare(const u16 idx1, const u16 idx2)
{
	const CPoly &cp1 = gfx3d.clippedPolyUnsortedList[idx1];
	const CPoly &cp2 = gfx3d.clippedPolyUnsortedList[idx2];
	
	const float &y1Max = gfx3d.rawPolySortYMax[cp1.index];
	const float &y2Max = gfx3d.rawPolySortYMax[cp2.index];
	
	const float &y1Min = gfx3d.rawPolySortYMin[cp1.index];
	const float &y2Min = gfx3d.rawPolySortYMin[cp2.index];
	
	//this may be verified by checking the game create menus in harvest moon island of happiness
	//also the buttons in the knights in the nightmare frontend depend on this and the perspective division
	if (y1Max != y2Max)
		return (y1Max < y2Max);
	if (y1Min != y2Min)
		return (y1Min < y2Min);
	
	//notably, the main shop interface in harvest moon will not have a correct RTN button
	//i think this is due to a math error rounding its position to one pixel too high and it popping behind
	//the bar that it sits on.
	//everything else in all the other menus that I could find looks right..
	
	//make sure we respect the game's ordering in cases of complete ties
	//this makes it a stable sort.
	//this must be a stable sort or else advance wars DOR will flicker in the main map mode
	return (idx1 < idx2);
}

template <ClipperMode CLIPPERMODE>
size_t gfx3d_PerformClipping(const GFX3D_GeometryList &gList, CPoly *outCPolyUnsortedList)
{
	size_t clipCount = 0;
	PolygonType cpType = POLYGON_TYPE_UNDEFINED;
	
	for (size_t polyIndex = 0; polyIndex < gList.rawPolyCount; polyIndex++)
	{
		const POLY &rawPoly = gList.rawPolyList[polyIndex];
		
		const VERT *rawVerts[4] = {
			&gList.rawVertList[rawPoly.vertIndexes[0]],
			&gList.rawVertList[rawPoly.vertIndexes[1]],
			&gList.rawVertList[rawPoly.vertIndexes[2]],
			(rawPoly.type == POLYGON_TYPE_QUAD) ? &gList.rawVertList[rawPoly.vertIndexes[3]] : NULL
		};
		
		cpType = GFX3D_GenerateClippedPoly<CLIPPERMODE>(polyIndex, rawPoly.type, rawVerts, outCPolyUnsortedList[clipCount]);
		if (cpType != POLYGON_TYPE_UNDEFINED)
		{
			clipCount++;
		}
	}
	
	return clipCount;
}

void GFX3D_GenerateRenderLists(const ClipperMode clippingMode, const GFX3D_State &inState, GFX3D_GeometryList &outGList)
{
	const VERT *__restrict appliedVertList = outGList.rawVertList;
	
	switch (clippingMode)
	{
		case ClipperMode_Full:
			outGList.clippedPolyCount = gfx3d_PerformClipping<ClipperMode_Full>(outGList, gfx3d.clippedPolyUnsortedList);
			break;
			
		case ClipperMode_FullColorInterpolate:
			outGList.clippedPolyCount = gfx3d_PerformClipping<ClipperMode_FullColorInterpolate>(outGList, gfx3d.clippedPolyUnsortedList);
			break;
			
		case ClipperMode_DetermineClipOnly:
			outGList.clippedPolyCount = gfx3d_PerformClipping<ClipperMode_DetermineClipOnly>(outGList, gfx3d.clippedPolyUnsortedList);
			break;
	}

#ifdef _SHOW_VTX_COUNTERS
	max_polys = max((u32)outGList.rawPolyCount, max_polys);
	max_verts = max((u32)outGList.rawVertCount, max_verts);
	osd->addFixed(180, 20, "%i/%i", outGList.rawPolyCount, outGList.rawVertCount);		// current
	osd->addFixed(180, 35, "%i/%i", max_polys, max_verts);		// max
#endif
	
	//we need to sort the poly list with alpha polys last
	//first, look for opaque polys
	size_t ctr = 0;
	for (size_t i = 0; i < outGList.clippedPolyCount; i++)
	{
		const CPoly &clippedPoly = gfx3d.clippedPolyUnsortedList[i];
		if ( !GFX3D_IsPolyTranslucent(outGList.rawPolyList[clippedPoly.index]) )
			gfx3d.indexOfClippedPolyUnsortedList[ctr++] = i;
	}
	outGList.clippedPolyOpaqueCount = ctr;
	
	//then look for translucent polys
	for (size_t i = 0; i < outGList.clippedPolyCount; i++)
	{
		const CPoly &clippedPoly = gfx3d.clippedPolyUnsortedList[i];
		if ( GFX3D_IsPolyTranslucent(outGList.rawPolyList[clippedPoly.index]) )
			gfx3d.indexOfClippedPolyUnsortedList[ctr++] = i;
	}
	
	//find the min and max y values for each poly.
	//the w-division here is just an approximation to fix the shop in harvest moon island of happiness
	//also the buttons in the knights in the nightmare frontend depend on this
	//we process all polys here instead of just the opaque ones (which have been sorted to the beginning of the index list earlier) because
	//1. otherwise it is basically two passes through the list and will probably run slower than one pass through the list
	//2. most geometry is opaque which is always sorted anyway
	for (size_t i = 0; i < outGList.clippedPolyCount; i++)
	{
		const u16 rawPolyIndex = gfx3d.clippedPolyUnsortedList[i].index;
		const POLY &poly = outGList.rawPolyList[rawPolyIndex];
		
		// TODO: Possible divide by zero with the w-coordinate.
		// Is the vertex being read correctly? Is 0 a valid value for w?
		// If both of these questions answer to yes, then how does the NDS handle a NaN?
		// For now, simply prevent w from being zero.
		float verty = appliedVertList[poly.vertIndexes[0]].y;
		float vertw = (appliedVertList[poly.vertIndexes[0]].w != 0.0f) ? appliedVertList[poly.vertIndexes[0]].w : 0.00000001f;
		verty = 1.0f-(verty+vertw)/(2*vertw);
		gfx3d.rawPolySortYMin[rawPolyIndex] = verty;
		gfx3d.rawPolySortYMax[rawPolyIndex] = verty;
		
		for (size_t j = 1; j < (size_t)poly.type; j++)
		{
			verty = appliedVertList[poly.vertIndexes[j]].y;
			vertw = (appliedVertList[poly.vertIndexes[j]].w != 0.0f) ? appliedVertList[poly.vertIndexes[j]].w : 0.00000001f;
			verty = 1.0f-(verty+vertw)/(2*vertw);
			gfx3d.rawPolySortYMin[rawPolyIndex] = min(gfx3d.rawPolySortYMin[rawPolyIndex], verty);
			gfx3d.rawPolySortYMax[rawPolyIndex] = max(gfx3d.rawPolySortYMax[rawPolyIndex], verty);
		}
	}

	//now we have to sort the opaque polys by y-value.
	//(test case: harvest moon island of happiness character creator UI)
	//should this be done after clipping??
	std::sort(gfx3d.indexOfClippedPolyUnsortedList, gfx3d.indexOfClippedPolyUnsortedList + outGList.clippedPolyOpaqueCount, gfx3d_ysort_compare);
	
	if (inState.SWAP_BUFFERS.YSortMode == 0)
	{
		//if we are autosorting translucent polys, we need to do this also
		//TODO - this is unverified behavior. need a test case
		std::sort(gfx3d.indexOfClippedPolyUnsortedList + outGList.clippedPolyOpaqueCount, gfx3d.indexOfClippedPolyUnsortedList + outGList.clippedPolyCount, gfx3d_ysort_compare);
	}
	
	// Reorder the clipped polygon list to match our sorted index list.
	if (clippingMode == ClipperMode_DetermineClipOnly)
	{
		for (size_t i = 0; i < outGList.clippedPolyCount; i++)
		{
			outGList.clippedPolyList[i].index = gfx3d.clippedPolyUnsortedList[gfx3d.indexOfClippedPolyUnsortedList[i]].index;
		}
	}
	else
	{
		for (size_t i = 0; i < outGList.clippedPolyCount; i++)
		{
			outGList.clippedPolyList[i] = gfx3d.clippedPolyUnsortedList[gfx3d.indexOfClippedPolyUnsortedList[i]];
		}
	}
}

static void gfx3d_doFlush()
{
	//latch the current renderer and geometry engine states
	//NOTE: the geometry lists are copied elsewhere by another operation.
	//that's pretty annoying.
	gfx3d.appliedListIndex = gfx3d.pendingListIndex;
	gfx3d.appliedState = gfx3d.pendingState;
	
	// Finalize the geometry lists for our 3D renderers.
	GFX3D_GeometryList &appliedGList = gfx3d.gList[gfx3d.appliedListIndex];
	const ClipperMode clippingMode = CurrentRenderer->GetPreferredPolygonClippingMode();
	GFX3D_GenerateRenderLists(clippingMode, gfx3d.appliedState, appliedGList);
	
	//switch to the new lists
	gfx3d.pendingListIndex++;
	gfx3d.pendingListIndex &= 1;
	
	GFX3D_GeometryList &pendingGList = gfx3d.gList[gfx3d.pendingListIndex];
	pendingGList.rawPolyCount = 0;
	pendingGList.rawVertCount = 0;
	pendingGList.clippedPolyCount = 0;
	pendingGList.clippedPolyOpaqueCount = 0;

	if (driver->view3d->IsRunning())
	{
		viewer3D.frameNumber = currFrameCounter;
		viewer3D.state = gfx3d.appliedState;
		viewer3D.gList.rawVertCount = appliedGList.rawVertCount;
		viewer3D.gList.rawPolyCount = appliedGList.rawPolyCount;
		viewer3D.gList.clippedPolyCount = appliedGList.clippedPolyCount;
		viewer3D.gList.clippedPolyOpaqueCount = appliedGList.clippedPolyOpaqueCount;
		
		memcpy(viewer3D.gList.rawVertList, appliedGList.rawVertList, appliedGList.rawVertCount * sizeof(VERT));
		memcpy(viewer3D.gList.rawPolyList, appliedGList.rawPolyList, appliedGList.rawPolyCount * sizeof(POLY));
		memcpy(viewer3D.gList.clippedPolyList, appliedGList.clippedPolyList, appliedGList.clippedPolyCount * sizeof(CPoly));
		
		driver->view3d->NewFrame();
	}
	
	gfx3d.render3DFrameCount++;
	_isDrawPending = true;
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
	if (gfx3d.appliedState.DISP3DCNT.RearPlaneMode)
	{
		forceDraw = true;
	}
	
	if (!_isDrawPending && !forceDraw)
		return;

	if (skipFrame) return;

	_isDrawPending = false;
	
	GPU->GetEventHandler()->DidApplyRender3DSettingsBegin();
	
	const ClipperMode oldClippingMode = CurrentRenderer->GetPreferredPolygonClippingMode();
	GPU->Change3DRendererIfNeeded();
	const ClipperMode newClippingMode = CurrentRenderer->GetPreferredPolygonClippingMode();
	
	if (oldClippingMode != newClippingMode)
	{
		GFX3D_GenerateRenderLists(newClippingMode, gfx3d.appliedState, gfx3d.gList[gfx3d.appliedListIndex]);
	}
	
	CurrentRenderer->ApplyRenderingSettings(gfx3d.appliedState);
	GPU->GetEventHandler()->DidApplyRender3DSettingsEnd();
	
	GPU->GetEventHandler()->DidRender3DBegin();
	CurrentRenderer->SetRenderNeedsFinish(true);
	
	//the timing of powering on rendering may not be exactly right here.
	if (GPU->GetEngineMain()->GetEnableStateApplied() && nds.power_render)
	{
		CurrentRenderer->SetTextureProcessingProperties();
		CurrentRenderer->Render(gfx3d.appliedState, gfx3d.gList[gfx3d.appliedListIndex]);
	}
	else
	{
		CurrentRenderer->RenderPowerOff();
	}
}

//#define _3D_LOG

void gfx3d_sendCommandToFIFO(const u32 v)
{
	//printf("gxFIFO: send val=0x%08X, size=%03i (fifo)\n", val, gxFIFO.size);

	gxf_hardware.receive(v);
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
		MatrixCopy(dst, _mtxCurrent[MODE]);
	}
	else
	{
		switch (MODE)
		{
			case MATRIXMODE_PROJECTION:
				MatrixCopy(dst, mtxStackProjection[0]);
				break;
				
			case MATRIXMODE_POSITION:
				MatrixCopy(dst, mtxStackPosition[0]);
				break;
				
			case MATRIXMODE_POSITION_VECTOR:
				MatrixCopy(dst, mtxStackPositionVector[0]);
				break;
				
			case MATRIXMODE_TEXTURE:
				MatrixCopy(dst, mtxStackTexture[0]);
				break;
				
			default:
				break;
		}
	}
}

void gfx3d_glGetLightDirection(const size_t index, u32 &dst)
{
	dst = _regLightDirection[index];
}

void gfx3d_glGetLightColor(const size_t index, u32 &dst)
{
	dst = _regLightColor[index];
}

//http://www.opengl.org/documentation/specs/version1.1/glspec1.1/node17.html
//talks about the state required to process verts in quadlists etc. helpful ideas.
//consider building a little state structure that looks exactly like this describes

SFORMAT SF_GFX3D[]={
	{ "GCTL", 4, 1, &gfx3d.pendingState.DISP3DCNT},
	{ "GPAT", 4, 1, &_polyAttrInProcess},
	{ "GPAP", 4, 1, &_currentPolyAttr},
	{ "GINB", 4, 1, &gfx3d.inBeginLegacySave},
	{ "GTFM", 4, 1, &_currentPolyTexParam},
	{ "GTPA", 4, 1, &_currentPolyTexPalette},
	{ "GMOD", 4, 1, &_mtxMode},
	{ "GMTM", 4,16, _mtxTemporal},
	{ "GMCU", 4,64, _mtxCurrent},
	{ "ML4I", 1, 1, &_ML4x4ind},
	{ "ML3I", 1, 1, &_ML4x3ind},
	{ "MM4I", 1, 1, &_MM4x4ind},
	{ "MM3I", 1, 1, &_MM4x3ind},
	{ "MMxI", 1, 1, &_MM3x3ind},
	{ "GSCO", 2, 4, &_vtxCoord16},
	{ "GCOI", 1, 1, &_vtxCoord16CurrentIndex},
	{ "GVFM", 4, 1, &_vtxFormat},
	{ "GTRN", 4, 4, &_regTranslate},
	{ "GTRI", 1, 1, &_regTranslateCurrentIndex},
	{ "GSCA", 4, 4, &_regScale},
	{ "GSCI", 1, 1, &_regScaleCurrentIndex},
	{ "G_T_", 4, 1, &_regTexCoord.t},
	{ "G_S_", 4, 1, &_regTexCoord.s},
	{ "GL_T", 4, 1, &_texCoordTransformed.t},
	{ "GL_S", 4, 1, &_texCoordTransformed.s},
	{ "GLCM", 4, 1, &gfx3d.clCommandLegacySave},
	{ "GLIN", 4, 1, &gfx3d.clIndexLegacySave},
	{ "GLI2", 4, 1, &gfx3d.clIndex2LegacySave},
	{ "GLSB", 4, 1, &isSwapBuffers},
	{ "GLBT", 4, 1, &_BTind},
	{ "GLPT", 4, 1, &_PTind},
	{ "GLPC", 4, 4, gfx3d.PTcoordsLegacySave},
	{ "GBTC", 2, 6, &_BTcoords[0]},
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
	{ "GCOL", 1, 4, &_vtxColor555X},
	{ "GLCO", 4, 4, _regLightColor},
	{ "GLDI", 4, 4, &_regLightDirection},
	{ "GMDI", 2, 1, &_regDiffuse},
	{ "GMAM", 2, 1, &_regAmbient},
	{ "GMSP", 2, 1, &_regSpecular},
	{ "GMEM", 2, 1, &_regEmission},
	{ "GDRP", 4, 1, &gfx3d.isDrawPendingLegacySave},
	{ "GSET", 4, 1, &_legacyGFX3DStateSFormatPending.enableTexturing},
	{ "GSEA", 4, 1, &_legacyGFX3DStateSFormatPending.enableAlphaTest},
	{ "GSEB", 4, 1, &_legacyGFX3DStateSFormatPending.enableAlphaBlending},
	{ "GSEX", 4, 1, &_legacyGFX3DStateSFormatPending.enableAntialiasing},
	{ "GSEE", 4, 1, &_legacyGFX3DStateSFormatPending.enableEdgeMarking},
	{ "GSEC", 4, 1, &_legacyGFX3DStateSFormatPending.enableClearImage},
	{ "GSEF", 4, 1, &_legacyGFX3DStateSFormatPending.enableFog},
	{ "GSEO", 4, 1, &_legacyGFX3DStateSFormatPending.enableFogAlphaOnly},
	{ "GFSH", 4, 1, &_legacyGFX3DStateSFormatPending.fogShift},
	{ "GSSH", 4, 1, &_legacyGFX3DStateSFormatPending.toonShadingMode},
	{ "GSWB", 4, 1, &_legacyGFX3DStateSFormatPending.enableWDepth},
	{ "GSSM", 4, 1, &_legacyGFX3DStateSFormatPending.polygonTransparentSortMode},
	{ "GSAR", 1, 1, &_legacyGFX3DStateSFormatPending.alphaTestRef},
	{ "GSCC", 4, 1, &_legacyGFX3DStateSFormatPending.clearColor},
	{ "GSCD", 4, 1, &_legacyGFX3DStateSFormatPending.clearDepth},
	{ "GSFC", 4, 4,  _legacyGFX3DStateSFormatPending.fogColor},
	{ "GSFO", 4, 1, &_legacyGFX3DStateSFormatPending.fogOffset},
	{ "GST4", 2, 32, _legacyGFX3DStateSFormatPending.toonTable16},
	{ "GSSU", 1, 128, _legacyGFX3DStateSFormatPending.shininessTable},
	{ "GSAF", 4, 1, &_legacyGFX3DStateSFormatPending.activeFlushCommand},
	{ "GSPF", 4, 1, &_legacyGFX3DStateSFormatPending.pendingFlushCommand},

	{ "gSET", 4, 1, &_legacyGFX3DStateSFormatApplied.enableTexturing},
	{ "gSEA", 4, 1, &_legacyGFX3DStateSFormatApplied.enableAlphaTest},
	{ "gSEB", 4, 1, &_legacyGFX3DStateSFormatApplied.enableAlphaBlending},
	{ "gSEX", 4, 1, &_legacyGFX3DStateSFormatApplied.enableAntialiasing},
	{ "gSEE", 4, 1, &_legacyGFX3DStateSFormatApplied.enableEdgeMarking},
	{ "gSEC", 4, 1, &_legacyGFX3DStateSFormatApplied.enableClearImage},
	{ "gSEF", 4, 1, &_legacyGFX3DStateSFormatApplied.enableFog},
	{ "gSEO", 4, 1, &_legacyGFX3DStateSFormatApplied.enableFogAlphaOnly},
	{ "gFSH", 4, 1, &_legacyGFX3DStateSFormatApplied.fogShift},
	{ "gSSH", 4, 1, &_legacyGFX3DStateSFormatApplied.toonShadingMode},
	{ "gSWB", 4, 1, &_legacyGFX3DStateSFormatApplied.enableWDepth},
	{ "gSSM", 4, 1, &_legacyGFX3DStateSFormatApplied.polygonTransparentSortMode},
	{ "gSAR", 1, 1, &_legacyGFX3DStateSFormatApplied.alphaTestRef},
	{ "gSCC", 4, 1, &_legacyGFX3DStateSFormatApplied.clearColor},
	{ "gSCD", 4, 1, &_legacyGFX3DStateSFormatApplied.clearDepth},
	{ "gSFC", 4, 4,  _legacyGFX3DStateSFormatApplied.fogColor},
	{ "gSFO", 4, 1, &_legacyGFX3DStateSFormatApplied.fogOffset},
	{ "gST4", 2, 32, _legacyGFX3DStateSFormatApplied.toonTable16},
	{ "gSSU", 1, 128, _legacyGFX3DStateSFormatApplied.shininessTable},
	{ "gSAF", 4, 1, &_legacyGFX3DStateSFormatApplied.activeFlushCommand},
	{ "gSPF", 4, 1, &_legacyGFX3DStateSFormatApplied.pendingFlushCommand},

	{ "GSVP", 4, 1, &gfx3d.viewportLegacySave},
	{ "GSSI", 1, 1, &_shininessTableCurrentIndex},
	//------------------------
	{ "GTST", 1, 1, &_triStripToggle},
	{ "GTVC", 4, 1, &gfx3d.polyGenVtxCountLegacySave},
	{ "GTVM", 4, 4, gfx3d.polyGenVtxIndexLegacySave},
	{ "GTVF", 4, 1, &gfx3d.polyGenIsFirstCompletedLegacySave},
	{ "G3CX", 1, 4*GPU_FRAMEBUFFER_NATIVE_WIDTH*GPU_FRAMEBUFFER_NATIVE_HEIGHT, gfx3d.framebufferNativeSave},
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
			ColorspaceConvertBuffer6665To8888<false, false>((u32 *)CurrentRenderer->GetFramebuffer(), (u32 *)gfx3d.framebufferNativeSave, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		}
		else
		{
			ColorspaceCopyBuffer32<false, false>((u32 *)CurrentRenderer->GetFramebuffer(), (u32 *)gfx3d.framebufferNativeSave, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		}
	}
	else // Framebuffer is at a custom size
	{
		const FragmentColor *__restrict src = CurrentRenderer->GetFramebuffer();
		FragmentColor *__restrict dst = gfx3d.framebufferNativeSave;
		
		for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
		{
			const GPUEngineLineInfo &lineInfo = GPU->GetLineInfoAtIndex(l);
			CopyLineReduceHinted<0x3FFF, false, true, 4>(lineInfo, src, dst);
			src += lineInfo.pixelCount;
			dst += GPU_FRAMEBUFFER_NATIVE_WIDTH;
		}
		
		if (CurrentRenderer->GetColorFormat() == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvertBuffer6665To8888<false, false>((u32 *)gfx3d.framebufferNativeSave, (u32 *)gfx3d.framebufferNativeSave, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		}
	}
	
	// For save state compatibility
	gfx3d.inBeginLegacySave = (_inBegin) ? TRUE : FALSE;
	gfx3d.isDrawPendingLegacySave = (_isDrawPending) ? TRUE : FALSE;
	
	gfx3d.polyGenVtxCountLegacySave = (u32)_polyGenInfo.vtxCount;
	gfx3d.polyGenVtxIndexLegacySave[0] = (u32)_polyGenInfo.vtxIndex[0];
	gfx3d.polyGenVtxIndexLegacySave[1] = (u32)_polyGenInfo.vtxIndex[1];
	gfx3d.polyGenVtxIndexLegacySave[2] = (u32)_polyGenInfo.vtxIndex[2];
	gfx3d.polyGenVtxIndexLegacySave[3] = (u32)_polyGenInfo.vtxIndex[3];
	gfx3d.polyGenIsFirstCompletedLegacySave = (_polyGenInfo.isFirstPolyCompleted) ? TRUE : FALSE;
	
	gfx3d.PTcoordsLegacySave[0] = (float)_PTcoords.x / 4096.0f;
	gfx3d.PTcoordsLegacySave[1] = (float)_PTcoords.y / 4096.0f;
	gfx3d.PTcoordsLegacySave[2] = (float)_PTcoords.z / 4096.0f;
	gfx3d.PTcoordsLegacySave[3] = (float)_PTcoords.w / 4096.0f;
	
	_legacyGFX3DStateSFormatPending.enableTexturing     = (gfx3d.pendingState.DISP3DCNT.EnableTexMapping)    ? TRUE : FALSE;
	_legacyGFX3DStateSFormatPending.enableAlphaTest     = (gfx3d.pendingState.DISP3DCNT.EnableAlphaTest)     ? TRUE : FALSE;
	_legacyGFX3DStateSFormatPending.enableAlphaBlending = (gfx3d.pendingState.DISP3DCNT.EnableAlphaBlending) ? TRUE : FALSE;
	_legacyGFX3DStateSFormatPending.enableAntialiasing  = (gfx3d.pendingState.DISP3DCNT.EnableAntialiasing)  ? TRUE : FALSE;
	_legacyGFX3DStateSFormatPending.enableEdgeMarking   = (gfx3d.pendingState.DISP3DCNT.EnableEdgeMarking)   ? TRUE : FALSE;
	_legacyGFX3DStateSFormatPending.enableClearImage    = (gfx3d.pendingState.DISP3DCNT.RearPlaneMode)       ? TRUE : FALSE;
	_legacyGFX3DStateSFormatPending.enableFog           = (gfx3d.pendingState.DISP3DCNT.EnableFog)           ? TRUE : FALSE;
	_legacyGFX3DStateSFormatPending.enableFogAlphaOnly  = (gfx3d.pendingState.DISP3DCNT.FogOnlyAlpha)        ? TRUE : FALSE;
	_legacyGFX3DStateSFormatPending.fogShift            = gfx3d.pendingState.fogShift;
	_legacyGFX3DStateSFormatPending.toonShadingMode     = gfx3d.pendingState.DISP3DCNT.PolygonShading;
	_legacyGFX3DStateSFormatPending.enableWDepth        = (gfx3d.pendingState.SWAP_BUFFERS.DepthMode)        ? TRUE : FALSE;
	_legacyGFX3DStateSFormatPending.polygonTransparentSortMode = (gfx3d.pendingState.SWAP_BUFFERS.YSortMode) ? TRUE : FALSE;
	_legacyGFX3DStateSFormatPending.alphaTestRef        = gfx3d.pendingState.alphaTestRef;
	_legacyGFX3DStateSFormatPending.clearColor          = gfx3d.pendingState.clearColor;
	_legacyGFX3DStateSFormatPending.clearDepth          = gfx3d.pendingState.clearDepth;
	_legacyGFX3DStateSFormatPending.fogColor[0]         = gfx3d.pendingState.fogColor;
	_legacyGFX3DStateSFormatPending.fogColor[1]         = gfx3d.pendingState.fogColor;
	_legacyGFX3DStateSFormatPending.fogColor[2]         = gfx3d.pendingState.fogColor;
	_legacyGFX3DStateSFormatPending.fogColor[3]         = gfx3d.pendingState.fogColor;
	_legacyGFX3DStateSFormatPending.fogOffset           = gfx3d.pendingState.fogOffset;
	_legacyGFX3DStateSFormatPending.activeFlushCommand  = gfx3d.appliedState.SWAP_BUFFERS.value;
	_legacyGFX3DStateSFormatPending.pendingFlushCommand = gfx3d.pendingState.SWAP_BUFFERS.value;
	memcpy(_legacyGFX3DStateSFormatPending.toonTable16, gfx3d.pendingState.toonTable16, sizeof(gfx3d.pendingState.toonTable16));
	memcpy(_legacyGFX3DStateSFormatPending.shininessTable, gfx3d.pendingState.shininessTable, sizeof(gfx3d.pendingState.shininessTable));
	
	_legacyGFX3DStateSFormatApplied.enableTexturing     = (gfx3d.appliedState.DISP3DCNT.EnableTexMapping)    ? TRUE : FALSE;
	_legacyGFX3DStateSFormatApplied.enableAlphaTest     = (gfx3d.appliedState.DISP3DCNT.EnableAlphaTest)     ? TRUE : FALSE;
	_legacyGFX3DStateSFormatApplied.enableAlphaBlending = (gfx3d.appliedState.DISP3DCNT.EnableAlphaBlending) ? TRUE : FALSE;
	_legacyGFX3DStateSFormatApplied.enableAntialiasing  = (gfx3d.appliedState.DISP3DCNT.EnableAntialiasing)  ? TRUE : FALSE;
	_legacyGFX3DStateSFormatApplied.enableEdgeMarking   = (gfx3d.appliedState.DISP3DCNT.EnableEdgeMarking)   ? TRUE : FALSE;
	_legacyGFX3DStateSFormatApplied.enableClearImage    = (gfx3d.appliedState.DISP3DCNT.RearPlaneMode)       ? TRUE : FALSE;
	_legacyGFX3DStateSFormatApplied.enableFog           = (gfx3d.appliedState.DISP3DCNT.EnableFog)           ? TRUE : FALSE;
	_legacyGFX3DStateSFormatApplied.enableFogAlphaOnly  = (gfx3d.appliedState.DISP3DCNT.FogOnlyAlpha)        ? TRUE : FALSE;
	_legacyGFX3DStateSFormatApplied.fogShift            = gfx3d.appliedState.fogShift;
	_legacyGFX3DStateSFormatApplied.toonShadingMode     = gfx3d.appliedState.DISP3DCNT.PolygonShading;
	_legacyGFX3DStateSFormatApplied.enableWDepth        = (gfx3d.appliedState.SWAP_BUFFERS.DepthMode)        ? TRUE : FALSE;
	_legacyGFX3DStateSFormatApplied.polygonTransparentSortMode = (gfx3d.appliedState.SWAP_BUFFERS.YSortMode) ? TRUE : FALSE;
	_legacyGFX3DStateSFormatApplied.alphaTestRef        = gfx3d.appliedState.alphaTestRef;
	_legacyGFX3DStateSFormatApplied.clearColor          = gfx3d.appliedState.clearColor;
	_legacyGFX3DStateSFormatApplied.clearDepth          = gfx3d.appliedState.clearDepth;
	_legacyGFX3DStateSFormatApplied.fogColor[0]         = gfx3d.appliedState.fogColor;
	_legacyGFX3DStateSFormatApplied.fogColor[1]         = gfx3d.appliedState.fogColor;
	_legacyGFX3DStateSFormatApplied.fogColor[2]         = gfx3d.appliedState.fogColor;
	_legacyGFX3DStateSFormatApplied.fogColor[3]         = gfx3d.appliedState.fogColor;
	_legacyGFX3DStateSFormatApplied.fogOffset           = gfx3d.appliedState.fogOffset;
	_legacyGFX3DStateSFormatApplied.activeFlushCommand  = gfx3d.appliedState.SWAP_BUFFERS.value;
	_legacyGFX3DStateSFormatApplied.pendingFlushCommand = gfx3d.pendingState.SWAP_BUFFERS.value;
	memcpy(_legacyGFX3DStateSFormatApplied.toonTable16, gfx3d.appliedState.toonTable16, sizeof(gfx3d.appliedState.toonTable16));
	memcpy(_legacyGFX3DStateSFormatApplied.shininessTable, gfx3d.appliedState.shininessTable, sizeof(gfx3d.appliedState.shininessTable));
}

void gfx3d_savestate(EMUFILE &os)
{
	//version
	os.write_32LE(4);

	//dump the render lists
	os.write_32LE((u32)gfx3d.gList[gfx3d.pendingListIndex].rawVertCount);
	for (size_t i = 0; i < gfx3d.gList[gfx3d.pendingListIndex].rawVertCount; i++)
	{
		GFX3D_SaveStateVERT(gfx3d.gList[gfx3d.pendingListIndex].rawVertList[i], os);
	}
	
	os.write_32LE((u32)gfx3d.gList[gfx3d.pendingListIndex].rawPolyCount);
	for (size_t i = 0; i < gfx3d.gList[gfx3d.pendingListIndex].rawPolyCount; i++)
	{
		GFX3D_SaveStatePOLY(gfx3d.gList[gfx3d.pendingListIndex].rawPolyList[i], os);
		os.write_32LE(gfx3d.rawPolyViewportLegacySave[i].value);
		os.write_floatLE(gfx3d.rawPolySortYMin[i]);
		os.write_floatLE(gfx3d.rawPolySortYMax[i]);
	}

	// Write matrix stack data
	os.write_32LE(mtxStackIndex[MATRIXMODE_PROJECTION]);
	for (size_t j = 0; j < 16; j++)
	{
		os.write_32LE(mtxStackProjection[0][j]);
	}
	
	os.write_32LE(mtxStackIndex[MATRIXMODE_POSITION]);
	for (size_t i = 0; i < NDSMATRIXSTACK_COUNT(MATRIXMODE_POSITION); i++)
	{
		for (size_t j = 0; j < 16; j++)
		{
			os.write_32LE(mtxStackPosition[i][j]);
		}
	}
	
	os.write_32LE(mtxStackIndex[MATRIXMODE_POSITION_VECTOR]);
	for (size_t i = 0; i < NDSMATRIXSTACK_COUNT(MATRIXMODE_POSITION_VECTOR); i++)
	{
		for (size_t j = 0; j < 16; j++)
		{
			os.write_32LE(mtxStackPositionVector[i][j]);
		}
	}
	
	os.write_32LE(mtxStackIndex[MATRIXMODE_TEXTURE]);
	for (size_t j = 0; j < 16; j++)
	{
		os.write_32LE(mtxStackTexture[0][j]);
	}

	gxf_hardware.savestate(os);

	// evidently these need to be saved because we don't cache the matrix that would need to be used to properly regenerate them
	for (size_t i = 0; i < 4; i++)
	{
		os.write_32LE(_cacheLightDirection[i].x);
		os.write_32LE(_cacheLightDirection[i].y);
		os.write_32LE(_cacheLightDirection[i].z);
		os.write_32LE(_cacheLightDirection[i].w);
	}
	
	for (size_t i = 0; i < 4; i++)
	{
		os.write_32LE(_cacheHalfVector[i].x);
		os.write_32LE(_cacheHalfVector[i].y);
		os.write_32LE(_cacheHalfVector[i].z);
		os.write_32LE(_cacheHalfVector[i].w);
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

	gfx3d_parseCurrentDISP3DCNT();
	
	if (version >= 1)
	{
		u32 vertListCount32 = 0;
		u32 polyListCount32 = 0;
		
		is.read_32LE(vertListCount32);
		gfx3d.gList[gfx3d.pendingListIndex].rawVertCount = vertListCount32;
		gfx3d.gList[gfx3d.appliedListIndex].rawVertCount = vertListCount32;
		for (size_t i = 0; i < gfx3d.gList[gfx3d.appliedListIndex].rawVertCount; i++)
		{
			GFX3D_LoadStateVERT(gfx3d.gList[gfx3d.pendingListIndex].rawVertList[i], is);
			gfx3d.gList[gfx3d.appliedListIndex].rawVertList[i] = gfx3d.gList[gfx3d.pendingListIndex].rawVertList[i];
		}
		
		is.read_32LE(polyListCount32);
		gfx3d.gList[gfx3d.pendingListIndex].rawPolyCount = polyListCount32;
		gfx3d.gList[gfx3d.appliedListIndex].rawPolyCount = polyListCount32;
		for (size_t i = 0; i < gfx3d.gList[gfx3d.appliedListIndex].rawPolyCount; i++)
		{
			POLY &p = gfx3d.gList[gfx3d.pendingListIndex].rawPolyList[i];
			
			GFX3D_LoadStatePOLY(p, is);
			is.read_32LE(gfx3d.rawPolyViewportLegacySave[i].value);
			is.read_floatLE(gfx3d.rawPolySortYMin[i]);
			is.read_floatLE(gfx3d.rawPolySortYMax[i]);
			
			p.viewport = GFX3D_ViewportParse(gfx3d.rawPolyViewportLegacySave[i].value);
			
			gfx3d.gList[gfx3d.appliedListIndex].rawPolyList[i] = p;
		}
	}

	if (version >= 2)
	{
		// Read matrix stack data
		is.read_32LE(mtxStackIndex[MATRIXMODE_PROJECTION]);
		for (size_t j = 0; j < 16; j++)
		{
			is.read_32LE(mtxStackProjection[0][j]);
		}
		
		is.read_32LE(mtxStackIndex[MATRIXMODE_POSITION]);
		for (size_t i = 0; i < NDSMATRIXSTACK_COUNT(MATRIXMODE_POSITION); i++)
		{
			for (size_t j = 0; j < 16; j++)
			{
				is.read_32LE(mtxStackPosition[i][j]);
			}
		}
		
		is.read_32LE(mtxStackIndex[MATRIXMODE_POSITION_VECTOR]);
		for (size_t i = 0; i < NDSMATRIXSTACK_COUNT(MATRIXMODE_POSITION_VECTOR); i++)
		{
			for (size_t j = 0; j < 16; j++)
			{
				is.read_32LE(mtxStackPositionVector[i][j]);
			}
		}
		
		is.read_32LE(mtxStackIndex[MATRIXMODE_TEXTURE]);
		for (size_t j = 0; j < 16; j++)
		{
			is.read_32LE(mtxStackTexture[0][j]);
		}
	}

	if (version >= 3)
	{
		gxf_hardware.loadstate(is);
	}

	if (version >= 4)
	{
		for (size_t i = 0; i < 4; i++)
		{
			is.read_32LE(_cacheLightDirection[i].x);
			is.read_32LE(_cacheLightDirection[i].y);
			is.read_32LE(_cacheLightDirection[i].z);
			is.read_32LE(_cacheLightDirection[i].w);
		}
		
		for (size_t i = 0; i < 4; i++)
		{
			is.read_32LE(_cacheHalfVector[i].x);
			is.read_32LE(_cacheHalfVector[i].y);
			is.read_32LE(_cacheHalfVector[i].z);
			is.read_32LE(_cacheHalfVector[i].w);
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
					ColorspaceConvertBuffer8888To6665<false, false>((u32 *)gfx3d.framebufferNativeSave, (u32 *)CurrentRenderer->GetFramebuffer(), GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				}
				else
				{
					ColorspaceCopyBuffer32<false, false>((u32 *)gfx3d.framebufferNativeSave, (u32 *)CurrentRenderer->GetFramebuffer(), GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				}
			}
			else // Framebuffer is at a custom size
			{
				if (CurrentRenderer->GetColorFormat() == NDSColorFormat_BGR666_Rev)
				{
					ColorspaceConvertBuffer8888To6665<false, false>((u32 *)gfx3d.framebufferNativeSave, (u32 *)gfx3d.framebufferNativeSave, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				}
				
				const FragmentColor *__restrict src = gfx3d.framebufferNativeSave;
				FragmentColor *__restrict dst = CurrentRenderer->GetFramebuffer();
				
				for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
				{
					const GPUEngineLineInfo &lineInfo = GPU->GetLineInfoAtIndex(l);
					CopyLineExpandHinted<0x3FFF, true, false, true, 4>(lineInfo, src, dst);
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
	
	// For save state compatibility
	const GPU_IOREG &GPUREG = GPU->GetEngineMain()->GetIORegisterMap();
	const GFX3D_IOREG &GFX3DREG = GFX3D_GetIORegisterMap();
	
	_inBegin = (gfx3d.inBeginLegacySave) ? true : false;
	_isDrawPending = (gfx3d.isDrawPendingLegacySave) ? true : false;
	
	_polyGenInfo.vtxCount = (size_t)gfx3d.polyGenVtxCountLegacySave;
	_polyGenInfo.vtxIndex[0] = (u16)gfx3d.polyGenVtxIndexLegacySave[0];
	_polyGenInfo.vtxIndex[1] = (u16)gfx3d.polyGenVtxIndexLegacySave[1];
	_polyGenInfo.vtxIndex[2] = (u16)gfx3d.polyGenVtxIndexLegacySave[2];
	_polyGenInfo.vtxIndex[3] = (u16)gfx3d.polyGenVtxIndexLegacySave[3];
	_polyGenInfo.isFirstPolyCompleted = (gfx3d.polyGenIsFirstCompletedLegacySave) ? true : false;
	
	_PTcoords.x = (s32)((gfx3d.PTcoordsLegacySave[0] * 4096.0f) + 0.000000001f);
	_PTcoords.y = (s32)((gfx3d.PTcoordsLegacySave[1] * 4096.0f) + 0.000000001f);
	_PTcoords.z = (s32)((gfx3d.PTcoordsLegacySave[2] * 4096.0f) + 0.000000001f);
	_PTcoords.w = (s32)((gfx3d.PTcoordsLegacySave[3] * 4096.0f) + 0.000000001f);
	
	gfx3d.viewport = GFX3D_ViewportParse(GFX3DREG.VIEWPORT.value);
	gfx3d.viewportLegacySave = GFX3DREG.VIEWPORT;
	
	gfx3d.pendingState.DISP3DCNT = GPUREG.DISP3DCNT;
	gfx3d_parseCurrentDISP3DCNT();
	
	gfx3d.pendingState.SWAP_BUFFERS = GFX3DREG.SWAP_BUFFERS;
	gfx3d.pendingState.clearColor = _legacyGFX3DStateSFormatPending.clearColor;
	gfx3d.pendingState.clearDepth = _legacyGFX3DStateSFormatPending.clearDepth;
	gfx3d.pendingState.fogColor = _legacyGFX3DStateSFormatPending.fogColor[0];
	gfx3d.pendingState.fogOffset = _legacyGFX3DStateSFormatPending.fogOffset;
	gfx3d.pendingState.alphaTestRef = _legacyGFX3DStateSFormatPending.alphaTestRef;
	memcpy(gfx3d.pendingState.toonTable16, _legacyGFX3DStateSFormatPending.toonTable16, sizeof(gfx3d.pendingState.toonTable16));
	memcpy(gfx3d.pendingState.shininessTable, _legacyGFX3DStateSFormatPending.shininessTable, sizeof(gfx3d.pendingState.shininessTable));
	memcpy(gfx3d.pendingState.edgeMarkColorTable, GFX3DREG.EDGE_COLOR, sizeof(GFX3DREG.EDGE_COLOR));
	memcpy(gfx3d.pendingState.fogDensityTable, GFX3DREG.FOG_TABLE, sizeof(GFX3DREG.FOG_TABLE));
	
	gfx3d.appliedState.DISP3DCNT.value               = 0;
	gfx3d.appliedState.DISP3DCNT.EnableTexMapping    = (_legacyGFX3DStateSFormatApplied.enableTexturing)     ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.EnableAlphaTest     = (_legacyGFX3DStateSFormatApplied.enableAlphaTest)     ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.EnableAlphaBlending = (_legacyGFX3DStateSFormatApplied.enableAlphaBlending) ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.EnableAntialiasing  = (_legacyGFX3DStateSFormatApplied.enableAntialiasing)  ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.EnableEdgeMarking   = (_legacyGFX3DStateSFormatApplied.enableEdgeMarking)   ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.RearPlaneMode       = (_legacyGFX3DStateSFormatApplied.enableClearImage)    ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.EnableFog           = (_legacyGFX3DStateSFormatApplied.enableFog)           ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.FogOnlyAlpha        = (_legacyGFX3DStateSFormatApplied.enableFogAlphaOnly)  ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.PolygonShading      = (_legacyGFX3DStateSFormatApplied.toonShadingMode)     ? 1 : 0;
	gfx3d.appliedState.SWAP_BUFFERS.value            = 0;
	gfx3d.appliedState.SWAP_BUFFERS.DepthMode        = (_legacyGFX3DStateSFormatApplied.enableWDepth)        ? 1 : 0;
	gfx3d.appliedState.SWAP_BUFFERS.YSortMode        = (_legacyGFX3DStateSFormatApplied.polygonTransparentSortMode) ? 1 : 0;
	gfx3d.appliedState.fogShift                      = _legacyGFX3DStateSFormatApplied.fogShift;
	gfx3d.appliedState.clearColor                    = _legacyGFX3DStateSFormatApplied.clearColor;
	gfx3d.appliedState.clearDepth                    = _legacyGFX3DStateSFormatApplied.clearDepth;
	gfx3d.appliedState.fogColor                      = _legacyGFX3DStateSFormatApplied.fogColor[0];
	gfx3d.appliedState.fogOffset                     = _legacyGFX3DStateSFormatApplied.fogOffset;
	gfx3d.appliedState.alphaTestRef                  = _legacyGFX3DStateSFormatApplied.alphaTestRef;
	gfx3d.appliedState.SWAP_BUFFERS.value            = _legacyGFX3DStateSFormatApplied.activeFlushCommand;
	memcpy(gfx3d.appliedState.toonTable16, _legacyGFX3DStateSFormatApplied.toonTable16, sizeof(gfx3d.appliedState.toonTable16));
	memcpy(gfx3d.appliedState.shininessTable, _legacyGFX3DStateSFormatApplied.shininessTable, sizeof(gfx3d.appliedState.shininessTable));
	memcpy(gfx3d.appliedState.edgeMarkColorTable, GFX3DREG.EDGE_COLOR, sizeof(GFX3DREG.EDGE_COLOR));
	memcpy(gfx3d.appliedState.fogDensityTable, GFX3DREG.FOG_TABLE, sizeof(GFX3DREG.FOG_TABLE));
}

void gfx3d_parseCurrentDISP3DCNT()
{
	const IOREG_DISP3DCNT &DISP3DCNT = gfx3d.pendingState.DISP3DCNT;
	
	// According to GBATEK, values greater than 10 force FogStep (0x400 >> FogShiftSHR) to become 0.
	// So set FogShiftSHR to 11 in this case so that calculations using (0x400 >> FogShiftSHR) will
	// equal 0.
	gfx3d.pendingState.fogShift = (DISP3DCNT.FogShiftSHR <= 10) ? DISP3DCNT.FogShiftSHR : 11;
}

const GFX3D_IOREG& GFX3D_GetIORegisterMap()
{
	return *_GFX3D_IORegisterMap;
}

void ParseReg_DISP3DCNT()
{
	const IOREG_DISP3DCNT &DISP3DCNT = GPU->GetEngineMain()->GetIORegisterMap().DISP3DCNT;
	
	if (gfx3d.pendingState.DISP3DCNT.value == DISP3DCNT.value)
	{
		return;
	}
	
	gfx3d.pendingState.DISP3DCNT.value = DISP3DCNT.value;
	gfx3d_parseCurrentDISP3DCNT();
}

bool GFX3D_IsPolyWireframe(const POLY &p)
{
	return (p.attribute.Alpha == 0);
}

bool GFX3D_IsPolyOpaque(const POLY &p)
{
	return (p.attribute.Alpha == 31);
}

bool GFX3D_IsPolyTranslucent(const POLY &p)
{
	// First, check if the polygon is wireframe or opaque.
	// If neither, then it must be translucent.
	if ( !GFX3D_IsPolyWireframe(p) && !GFX3D_IsPolyOpaque(p) )
	{
		return true;
	}
	
	// Also check for translucent texture format.
	const NDSTextureFormat texFormat = (NDSTextureFormat)p.texParam.PackedFormat;
	const PolygonMode polyMode = (PolygonMode)p.attribute.Mode;
	
	//a5i3 or a3i5 -> translucent
	return ( ((texFormat == TEXMODE_A3I5) || (texFormat == TEXMODE_A5I3)) &&
	         (polyMode != POLYGON_MODE_DECAL) &&
	         (polyMode != POLYGON_MODE_SHADOW) );
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

template <typename T>
static T GFX3D_ClipPointInterpolate(const float ratio, const T& x0, const T& x1)
{
	return (T)(x0 + (float)(x1-x0) * ratio);
}

//http://www.cs.berkeley.edu/~ug/slide/pipeline/assignments/as6/discussion.shtml
template <ClipperMode CLIPPERMODE, int COORD, int WHICH>
static FORCEINLINE void GFX3D_ClipPoint(const VERT &insideVtx, const VERT &outsideVtx, VERT &outClippedVtx)
{
	const float coord_inside  = insideVtx.coord[COORD];
	const float coord_outside = outsideVtx.coord[COORD];
	const float w_inside  = (WHICH == -1) ? -insideVtx.coord[3]  : insideVtx.coord[3];
	const float w_outside = (WHICH == -1) ? -outsideVtx.coord[3] : outsideVtx.coord[3];
	const float t = (coord_inside - w_inside) / ((w_outside-w_inside) - (coord_outside-coord_inside));
	
#define INTERP(X) outClippedVtx . X = GFX3D_ClipPointInterpolate (t, insideVtx. X ,outsideVtx. X )
	
	INTERP(coord[0]); INTERP(coord[1]); INTERP(coord[2]); INTERP(coord[3]);
	
	switch (CLIPPERMODE)
	{
		case ClipperMode_Full:
			INTERP(texcoord[0]); INTERP(texcoord[1]);
			INTERP(color[0]); INTERP(color[1]); INTERP(color[2]);
			outClippedVtx.rf = (float)outClippedVtx.r;
			outClippedVtx.gf = (float)outClippedVtx.g;
			outClippedVtx.bf = (float)outClippedVtx.b;
			outClippedVtx.af = (float)outClippedVtx.a;
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
		outClippedVtx.coord[COORD] = -outClippedVtx.coord[3];
	else
		outClippedVtx.coord[COORD] = outClippedVtx.coord[3];
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

	void clipVert(const VERT &vert)
	{
		if (m_prevVert)
			this->clipSegmentVsPlane(*m_prevVert, vert);
		else
			m_firstVert = (VERT *)&vert;
		
		m_prevVert = (VERT *)&vert;
	}

	// closes the loop and returns the number of clipped output verts
	int finish()
	{
		this->clipVert(*m_firstVert);
		return m_next.finish();
	}

private:
	VERT* m_prevVert;
	VERT* m_firstVert;
	NEXT& m_next;
	
	FORCEINLINE void clipSegmentVsPlane(const VERT &vert0, const VERT &vert1)
	{
		const float *vert0coord = vert0.coord;
		const float *vert1coord = vert1.coord;
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
			GFX3D_ClipPoint<CLIPPERMODE, COORD, WHICH>(vert0, vert1, scratchClipVerts[numScratchClipVerts]);
			m_next.clipVert(scratchClipVerts[numScratchClipVerts++]);
		}

		//entering volume: insert clipped point and the next (interior) point
		if (out0 && !out1)
		{
			CLIPLOG(" entering\n");
			assert((u32)numScratchClipVerts < MAX_SCRATCH_CLIP_VERTS);
			GFX3D_ClipPoint<CLIPPERMODE, COORD, WHICH>(vert1, vert0, scratchClipVerts[numScratchClipVerts]);
			m_next.clipVert(scratchClipVerts[numScratchClipVerts++]);
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
	
	void clipVert(const VERT &vert)
	{
		assert((u32)m_numVerts < MAX_CLIPPED_VERTS);
		*m_nextDestVert++ = vert;
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

template <ClipperMode CLIPPERMODE>
PolygonType GFX3D_GenerateClippedPoly(const u16 rawPolyIndex, const PolygonType rawPolyType, const VERT *(&rawVtx)[4], CPoly &outCPoly)
{
	CLIPLOG("==Begin poly==\n");

	PolygonType outClippedType;
	numScratchClipVerts = 0;
	
	switch (CLIPPERMODE)
	{
		case ClipperMode_Full:
		{
			clipper1.init(outCPoly.clipVerts);
			for (size_t i = 0; i < (size_t)rawPolyType; i++)
				clipper1.clipVert(*rawVtx[i]);
			
			outClippedType = (PolygonType)clipper1.finish();
			break;
		}
			
		case ClipperMode_FullColorInterpolate:
		{
			clipper1i.init(outCPoly.clipVerts);
			for (size_t i = 0; i < (size_t)rawPolyType; i++)
				clipper1i.clipVert(*rawVtx[i]);
			
			outClippedType = (PolygonType)clipper1i.finish();
			break;
		}
			
		case ClipperMode_DetermineClipOnly:
		{
			clipper1d.init(outCPoly.clipVerts);
			for (size_t i = 0; i < (size_t)rawPolyType; i++)
				clipper1d.clipVert(*rawVtx[i]);
			
			outClippedType = (PolygonType)clipper1d.finish();
			break;
		}
	}
	
	assert((u32)outClippedType < MAX_CLIPPED_VERTS);
	if (outClippedType < POLYGON_TYPE_TRIANGLE)
	{
		//a totally clipped poly. discard it.
		//or, a degenerate poly. we're not handling these right now
		return POLYGON_TYPE_UNDEFINED;
	}
	
	outCPoly.index = rawPolyIndex;
	outCPoly.type = outClippedType;
	
	return outClippedType;
}
