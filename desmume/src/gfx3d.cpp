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

Viewer3D_State viewer3D;

static GFX3D_IOREG *_GFX3D_IORegisterMap = NULL;
static NDSGeometryEngine _gEngine;
static GFX3D gfx3d;

//tables that are provided to anyone
CACHE_ALIGN u32 dsDepthExtend_15bit_to_24bit[32768];

// "Freelook" related things
int freelookMode = 0;
s32 freelookMatrix[16];

//------------------

#define RENDER_FRONT_SURFACE 0x80
#define RENDER_BACK_SURFACE 0X40

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

GFX3D_Viewport GFX3D_ViewportParse(const IOREG_VIEWPORT reg)
{
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

GFX3D_Viewport GFX3D_ViewportParse(const u32 param)
{
	const IOREG_VIEWPORT regViewport = { param };
	return GFX3D_ViewportParse(regViewport);
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

void GFX3D_SaveStateVertex(const NDSVertex &vtx, EMUFILE &os)
{
	// The legacy vertex format uses coordinates in
	// normalized floating-point.
	os.write_floatLE((float)vtx.position.x / 4096.0f);
	os.write_floatLE((float)vtx.position.y / 4096.0f);
	os.write_floatLE((float)vtx.position.z / 4096.0f);
	os.write_floatLE((float)vtx.position.w / 4096.0f);
	os.write_floatLE((float)vtx.texCoord.s / 16.0f);
	os.write_floatLE((float)vtx.texCoord.t / 16.0f);
	os.write_u8(vtx.color.r);
	os.write_u8(vtx.color.g);
	os.write_u8(vtx.color.b);
	os.write_floatLE((float)vtx.color.r);
	os.write_floatLE((float)vtx.color.g);
	os.write_floatLE((float)vtx.color.b);
}

void GFX3D_LoadStateVertex(NDSVertex &vtx, EMUFILE &is)
{
	// Read the legacy vertex format.
	Vector4f32 legacyPosition;
	Vector2f32 legacyTexCoord;
	Color4u8 legacyColor;
	Color3f32 legacyColorFloat;
	
	is.read_floatLE(legacyPosition.x);
	is.read_floatLE(legacyPosition.y);
	is.read_floatLE(legacyPosition.z);
	is.read_floatLE(legacyPosition.w);
	is.read_floatLE(legacyTexCoord.u);
	is.read_floatLE(legacyTexCoord.v);
	is.read_u8(legacyColor.r);
	is.read_u8(legacyColor.g);
	is.read_u8(legacyColor.b);
	is.read_floatLE(legacyColorFloat.r);
	is.read_floatLE(legacyColorFloat.g);
	is.read_floatLE(legacyColorFloat.b);
	
	// Convert the legacy vertex format to the new one.
	vtx.position.x = (s32)( (legacyPosition.x * 4096.0f) + 0.5f );
	vtx.position.y = (s32)( (legacyPosition.y * 4096.0f) + 0.5f );
	vtx.position.z = (s32)( (legacyPosition.z * 4096.0f) + 0.5f );
	vtx.position.w = (s32)( (legacyPosition.w * 4096.0f) + 0.5f );
	vtx.texCoord.s = (s32)( (legacyTexCoord.u * 16.0f) + 0.5f );
	vtx.texCoord.t = (s32)( (legacyTexCoord.v * 16.0f) + 0.5f );
	vtx.color.r = legacyColor.r;
	vtx.color.g = legacyColor.g;
	vtx.color.b = legacyColor.b;
	vtx.color.a = 0;
}

void gfx3d_init()
{
	_GFX3D_IORegisterMap = (GFX3D_IOREG *)(&MMU.ARM9_REG[0x0320]);
	
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
	
	memset(gxPIPE.cmd, 0, sizeof(gxPIPE.cmd));
	memset(gxPIPE.param, 0, sizeof(gxPIPE.param));
	memset(gfx3d.framebufferNativeSave, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u32));
	
	// Init for save state compatibility
	_GFX3D_IORegisterMap->VIEWPORT.X1 = 0;
	_GFX3D_IORegisterMap->VIEWPORT.Y1 = 0;
	_GFX3D_IORegisterMap->VIEWPORT.X2 = 255;
	_GFX3D_IORegisterMap->VIEWPORT.Y2 = 191;

	GFX_PIPEclear();
	GFX_FIFOclear();
	
	_gEngine.Reset();
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


NDSGeometryEngine::NDSGeometryEngine()
{
	__Init();
}

void NDSGeometryEngine::__Init()
{
	static const Vector2s16 zeroVec2s16 = {0, 0};
	static const Vector3s16 zeroVec3s16 = {0, 0, 0};
	static const Vector4s32 zeroVec4s32 = {0, 0, 0, 0};
	
	_mtxCurrentMode = MATRIXMODE_PROJECTION;
	
	MatrixInit(_mtxCurrent[MATRIXMODE_PROJECTION]);
	MatrixInit(_mtxCurrent[MATRIXMODE_POSITION]);
	MatrixInit(_mtxCurrent[MATRIXMODE_POSITION_VECTOR]);
	MatrixInit(_mtxCurrent[MATRIXMODE_TEXTURE]);
	
	MatrixInit(_mtxStackProjection[0]);
	for (size_t i = 0; i < NDSMATRIXSTACK_COUNT(MATRIXMODE_POSITION);        i++) { MatrixInit(_mtxStackPosition[i]);       }
	for (size_t i = 0; i < NDSMATRIXSTACK_COUNT(MATRIXMODE_POSITION_VECTOR); i++) { MatrixInit(_mtxStackPositionVector[i]); }
	MatrixInit(_mtxStackTexture[0]);
	
	_vecScale = zeroVec4s32;
	_vecTranslate = zeroVec4s32;
	
	_mtxStackIndex[MATRIXMODE_PROJECTION] = 0;
	_mtxStackIndex[MATRIXMODE_POSITION] = 0;
	_mtxStackIndex[MATRIXMODE_POSITION_VECTOR] = 0;
	_mtxStackIndex[MATRIXMODE_TEXTURE] = 0;
	_mtxLoad4x4PendingIndex = 0;
	_mtxLoad4x3PendingIndex = 0;
	_mtxMultiply4x4TempIndex = 0;
	_mtxMultiply4x3TempIndex = 0;
	_mtxMultiply3x3TempIndex = 0;
	_vecScaleCurrentIndex = 0;
	_vecTranslateCurrentIndex = 0;
	
	_polyAttribute.value = 0;
	_texParam.value = 0;
	_texPalette = 0;
	
	_vtxColor15 = 0x7FFF;
	
	_vtxColor555X.r = 31;
	_vtxColor555X.g = 31;
	_vtxColor555X.b = 31;
	_vtxColor555X.a = 0;
	
	_vtxColor666X.r = 63;
	_vtxColor666X.g = 63;
	_vtxColor666X.b = 63;
	_vtxColor666X.a = 0;
	
	_vtxCoord16 = zeroVec3s16;
	_vecNormal = zeroVec4s32;
	
	_regViewport.X1 = 0;
	_regViewport.Y1 = 0;
	_regViewport.X2 = GPU_FRAMEBUFFER_NATIVE_WIDTH - 1;
	_regViewport.Y2 = GPU_FRAMEBUFFER_NATIVE_HEIGHT - 1;
	
	_currentViewport.x = 0;
	_currentViewport.y = 0;
	_currentViewport.width = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_currentViewport.height = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	_texCoordTransformMode = TextureTransformationMode_None;
	_texCoord16 = zeroVec2s16;
	_texCoordTransformed.s = (s32)_texCoord16.s;
	_texCoordTransformed.t = (s32)_texCoord16.t;
	
	_doesViewportNeedUpdate = true;
	_doesVertexColorNeedUpdate = true;
	_doesTransformedTexCoordsNeedUpdate = true;
	_vtxCoord16CurrentIndex = 0;
	
	_inBegin = false;
	_vtxFormat = GFX3D_TRIANGLES;
	_vtxCount = 0;
	_vtxIndex[0] = 0;
	_vtxIndex[1] = 0;
	_vtxIndex[2] = 0;
	_vtxIndex[3] = 0;
	_isGeneratingFirstPolyOfStrip = true;
	_generateTriangleStripIndexToggle = false;
	
	_regLightColor[0] = 0;
	_regLightColor[1] = 0;
	_regLightColor[2] = 0;
	_regLightColor[3] = 0;
	
	_regLightDirection[0] = 0;
	_regLightDirection[1] = 0;
	_regLightDirection[2] = 0;
	_regLightDirection[3] = 0;
	
	_vecLightDirectionTransformed[0] = zeroVec4s32;
	_vecLightDirectionTransformed[1] = zeroVec4s32;
	_vecLightDirectionTransformed[2] = zeroVec4s32;
	_vecLightDirectionTransformed[3] = zeroVec4s32;
	
	_vecLightDirectionHalfNegative[0] = zeroVec4s32;
	_vecLightDirectionHalfNegative[1] = zeroVec4s32;
	_vecLightDirectionHalfNegative[2] = zeroVec4s32;
	_vecLightDirectionHalfNegative[3] = zeroVec4s32;
	
	_doesLightHalfVectorNeedUpdate[0] = true;
	_doesLightHalfVectorNeedUpdate[1] = true;
	_doesLightHalfVectorNeedUpdate[2] = true;
	_doesLightHalfVectorNeedUpdate[3] = true;

	_boxTestCoordCurrentIndex = 0;
	_positionTestCoordCurrentIndex = 0;
	
	_boxTestCoord16[0] = 0;
	_boxTestCoord16[1] = 0;
	_boxTestCoord16[2] = 0;
	_boxTestCoord16[3] = 0;
	_boxTestCoord16[4] = 0;
	_boxTestCoord16[5] = 0;
	
	_positionTestVtx32.x = 0;
	_positionTestVtx32.y = 0;
	_positionTestVtx32.z = 0;
	_positionTestVtx32.w = (s32)(1 << 12);
	
	_regDiffuse  = 0;
	_regAmbient  = 0;
	_regSpecular = 0;
	_regEmission = 0;
	_shininessTablePendingIndex = 0;
	memset(_shininessTablePending, 0, sizeof(_shininessTablePending));
	memset(_shininessTableApplied, 0, sizeof(_shininessTableApplied));
	
	_lastMtxMultCommand = LastMtxMultCommand_4x4;
}

void NDSGeometryEngine::_UpdateTransformedTexCoordsIfNeeded()
{
	if (this->_texCoordTransformMode != TextureTransformationMode_None)
	{
		this->_doesTransformedTexCoordsNeedUpdate = true;
	}
}

void NDSGeometryEngine::Reset()
{
	this->__Init();
}

MatrixMode NDSGeometryEngine::GetMatrixMode() const
{
	return this->_mtxCurrentMode;
}

u32 NDSGeometryEngine::GetLightDirectionRegisterAtIndex(const size_t i) const
{
	return this->_regLightDirection[i];
}

u32 NDSGeometryEngine::GetLightColorRegisterAtIndex(const size_t i) const
{
	return this->_regLightColor[i];
}

u8 NDSGeometryEngine::GetMatrixStackIndex(const MatrixMode whichMatrix) const
{
	if ( (whichMatrix == MATRIXMODE_POSITION) ||
	     (whichMatrix == MATRIXMODE_POSITION_VECTOR) )
	{
		return this->_mtxStackIndex[whichMatrix] & 0x1F;
	}
	
	// MATRIXMODE_PROJECTION || MATRIXMODE_TEXTURE
	return this->_mtxStackIndex[whichMatrix] & 0x01;
}

void NDSGeometryEngine::ResetMatrixStackPointer()
{
	this->_mtxStackIndex[MATRIXMODE_PROJECTION] = 0;
}

void NDSGeometryEngine::SetMatrixMode(const u32 param)
{
	this->_mtxCurrentMode = (MatrixMode)(param & 0x00000003);
}

bool NDSGeometryEngine::SetCurrentMatrixLoad4x4(const u32 param)
{
	this->_pendingMtxLoad4x4[this->_mtxLoad4x4PendingIndex] = (s32)param;
	this->_mtxLoad4x4PendingIndex++;
	
	if (this->_mtxLoad4x4PendingIndex < 16)
	{
		return false;
	}
	
	this->_mtxLoad4x4PendingIndex = 0;
	return true;
}

bool NDSGeometryEngine::SetCurrentMatrixLoad4x3(const u32 param)
{
	this->_pendingMtxLoad4x3[this->_mtxLoad4x3PendingIndex] = (s32)param;
	this->_mtxLoad4x3PendingIndex++;
	
	if ((this->_mtxLoad4x3PendingIndex & 0x03) == 3)
	{
		// For a 3 column matrix, every 4th value must be set to 0.
		this->_pendingMtxLoad4x3[this->_mtxLoad4x3PendingIndex] = 0;
		this->_mtxLoad4x3PendingIndex++;
	}
	
	if (this->_mtxLoad4x3PendingIndex < 16)
	{
		return false;
	}
	
	// The last value needs to be a fixed 20.12 value of 1.
	this->_pendingMtxLoad4x3[15] = (s32)(1 << 12);
	
	this->_mtxLoad4x3PendingIndex = 0;
	return true;
}

bool NDSGeometryEngine::SetCurrentMatrixMultiply4x4(const u32 param)
{
	this->_tempMtxMultiply4x4[this->_mtxMultiply4x4TempIndex] = (s32)param;
	this->_mtxMultiply4x4TempIndex++;
	this->_lastMtxMultCommand = LastMtxMultCommand_4x4;
	
	if (this->_mtxMultiply4x4TempIndex < 16)
	{
		return false;
	}
	
	this->_mtxMultiply4x4TempIndex = 0;
	return true;
}

bool NDSGeometryEngine::SetCurrentMatrixMultiply4x3(const u32 param)
{
	this->_tempMtxMultiply4x3[this->_mtxMultiply4x3TempIndex] = (s32)param;
	this->_mtxMultiply4x3TempIndex++;
	this->_lastMtxMultCommand = LastMtxMultCommand_4x3;
	
	if ((this->_mtxMultiply4x3TempIndex & 0x03) == 3)
	{
		// For a 3 column matrix, every 4th value must be set to 0.
		this->_tempMtxMultiply4x3[this->_mtxMultiply4x3TempIndex] = 0;
		this->_mtxMultiply4x3TempIndex++;
	}
	
	if (this->_mtxMultiply4x3TempIndex < 16)
	{
		return false;
	}
	
	// The last value needs to be a fixed 20.12 value of 1.
	this->_tempMtxMultiply4x3[15] = (s32)(1 << 12);
	
	this->_mtxMultiply4x3TempIndex = 0;
	return true;
}

bool NDSGeometryEngine::SetCurrentMatrixMultiply3x3(const u32 param)
{
	this->_tempMtxMultiply3x3[this->_mtxMultiply3x3TempIndex] = (s32)param;
	this->_mtxMultiply3x3TempIndex++;
	this->_lastMtxMultCommand = LastMtxMultCommand_3x3;
	
	if ((this->_mtxMultiply3x3TempIndex & 0x03) == 3)
	{
		// For a 3 column matrix, every 4th value must be set to 0.
		this->_tempMtxMultiply3x3[this->_mtxMultiply3x3TempIndex] = 0;
		this->_mtxMultiply3x3TempIndex++;
	}
	
	if (this->_mtxMultiply3x3TempIndex < 12)
	{
		return false;
	}
	
	// Fill in the last matrix row.
	this->_tempMtxMultiply3x3[12] = 0;
	this->_tempMtxMultiply3x3[13] = 0;
	this->_tempMtxMultiply3x3[14] = 0;
	this->_tempMtxMultiply3x3[15] = (s32)(1 << 12);
	
	this->_mtxMultiply3x3TempIndex = 0;
	return true;
}

bool NDSGeometryEngine::SetCurrentScaleVector(const u32 param)
{
	this->_vecScale.vec[this->_vecScaleCurrentIndex] = (s32)param;
	this->_vecScaleCurrentIndex++;

	if (this->_vecScaleCurrentIndex < 3)
	{
		return false;
	}
	
	this->_vecScaleCurrentIndex = 0;
	return true;
}

bool NDSGeometryEngine::SetCurrentTranslateVector(const u32 param)
{
	this->_vecTranslate.vec[this->_vecTranslateCurrentIndex] = (s32)param;
	this->_vecTranslateCurrentIndex++;

	if (this->_vecTranslateCurrentIndex < 3)
	{
		return false;
	}
	
	this->_vecTranslateCurrentIndex = 0;
	return true;
}

void NDSGeometryEngine::MatrixPush()
{
	//1. apply offset specified by push (well, it's always +1) to internal counter
	//2. mask that bit off to actually index the matrix for reading
	//3. SE is set depending on resulting internal counter

	//printf("%d %d %d %d -> ", mtxStack[0].position, mtxStack[1].position, mtxStack[2].position, mtxStack[3].position);
	//printf("PUSH mode: %d -> ", this->_mtxCurrentMode, mtxStack[this->_mtxCurrentMode].position);

	if (this->_mtxCurrentMode == MATRIXMODE_PROJECTION || this->_mtxCurrentMode == MATRIXMODE_TEXTURE)
	{
		if (this->_mtxCurrentMode == MATRIXMODE_PROJECTION)
		{
			MatrixCopy(this->_mtxStackProjection[0], this->_mtxCurrent[MATRIXMODE_PROJECTION]);
			
			u8 &i = this->_mtxStackIndex[MATRIXMODE_PROJECTION];
			if (i == 1)
			{
				MMU_new.gxstat.se = 1;
			}
			
			i += 1;
			i &= 1;

			this->UpdateMatrixProjectionLua();
		}
		else
		{
			MatrixCopy(this->_mtxStackTexture[0], this->_mtxCurrent[MATRIXMODE_TEXTURE]);
			
			u8 &i = this->_mtxStackIndex[MATRIXMODE_TEXTURE];
			if (i == 1)
			{
				MMU_new.gxstat.se = 1; //unknown if this applies to the texture matrix
			}
			
			i += 1;
			i &= 1;
		}
	}
	else
	{
		u8 &i = this->_mtxStackIndex[MATRIXMODE_POSITION];
		
		MatrixCopy(this->_mtxStackPosition[i & 0x1F], this->_mtxCurrent[MATRIXMODE_POSITION]);
		MatrixCopy(this->_mtxStackPositionVector[i & 0x1F], this->_mtxCurrent[MATRIXMODE_POSITION_VECTOR]);
		
		i += 1;
		i &= 0x3F;
		
		if (i >= 32)
		{
			MMU_new.gxstat.se = 1; //(not sure, this might be off by 1)
		}
	}

	//printf("%d %d %d %d\n",mtxStack[0].position,mtxStack[1].position,mtxStack[2].position,mtxStack[3].position);
}

void NDSGeometryEngine::MatrixPop(const u32 param)
{
	//1. apply offset specified by pop to internal counter
	//2. SE is set depending on resulting internal counter
	//3. mask that bit off to actually index the matrix for reading

	//printf("%d %d %d %d -> ", mtxStack[0].position, mtxStack[1].position, mtxStack[2].position, mtxStack[3].position);
	//printf("POP   (%d): mode: %d -> ",v, this->_mtxCurrentMode,mtxStack[this->_mtxCurrentMode].position);

	if (this->_mtxCurrentMode == MATRIXMODE_PROJECTION || this->_mtxCurrentMode == MATRIXMODE_TEXTURE)
	{
		//parameter is ignored and treated as sensible (always 1)
		
		if (this->_mtxCurrentMode == MATRIXMODE_PROJECTION)
		{
			u8 &i = this->_mtxStackIndex[MATRIXMODE_PROJECTION];
			i ^= 1;
			
			if (i == 1)
			{
				MMU_new.gxstat.se = 1;
			}
			
			MatrixCopy(this->_mtxCurrent[MATRIXMODE_PROJECTION], this->_mtxStackProjection[0]);
			this->UpdateMatrixProjectionLua();
		}
		else
		{
			u8 &i = this->_mtxStackIndex[MATRIXMODE_TEXTURE];
			i ^= 1;
			
			if (i == 1)
			{
				MMU_new.gxstat.se = 1; //unknown if this applies to the texture matrix
			}
			
			MatrixCopy(this->_mtxCurrent[MATRIXMODE_TEXTURE], this->_mtxStackTexture[0]);
			this->_UpdateTransformedTexCoordsIfNeeded();
		}
	}
	else
	{
		u8 &i = this->_mtxStackIndex[MATRIXMODE_POSITION];
		i -= (u8)(param & 0x0000003F);
		i &= 0x3F;
		
		if (i >= 32)
		{
			MMU_new.gxstat.se = 1; //(not sure, this might be off by 1)
		}
		
		MatrixCopy(this->_mtxCurrent[MATRIXMODE_POSITION], this->_mtxStackPosition[i & 0x1F]);
		MatrixCopy(this->_mtxCurrent[MATRIXMODE_POSITION_VECTOR], this->_mtxStackPositionVector[i & 0x1F]);
	}

	//printf("%d %d %d %d\n",mtxStack[0].position,mtxStack[1].position,mtxStack[2].position,mtxStack[3].position);
}

void NDSGeometryEngine::MatrixStore(const u32 param)
{
	//printf("%d %d %d %d -> ", mtxStack[0].position, mtxStack[1].position, mtxStack[2].position, mtxStack[3].position);
	//printf("STORE (%d): mode: %d -> ",v, this->_mtxCurrentMode, mtxStack[this->_mtxCurrentMode].position);

	if (this->_mtxCurrentMode == MATRIXMODE_PROJECTION || this->_mtxCurrentMode == MATRIXMODE_TEXTURE)
	{
		//parameter ignored and treated as sensible
		if (this->_mtxCurrentMode == MATRIXMODE_PROJECTION)
		{
			MatrixCopy(this->_mtxStackProjection[0], this->_mtxCurrent[MATRIXMODE_PROJECTION]);
			this->UpdateMatrixProjectionLua();
		}
		else
		{
			MatrixCopy(this->_mtxStackTexture[0], this->_mtxCurrent[MATRIXMODE_TEXTURE]);
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
		MatrixCopy(this->_mtxStackPosition[i], this->_mtxCurrent[MATRIXMODE_POSITION]);
		MatrixCopy(this->_mtxStackPositionVector[i], this->_mtxCurrent[MATRIXMODE_POSITION_VECTOR]);
	}

	//printf("%d %d %d %d\n", mtxStack[0].position, mtxStack[1].position, mtxStack[2].position, mtxStack[3].position);
}

void NDSGeometryEngine::MatrixRestore(const u32 param)
{
	if (this->_mtxCurrentMode == MATRIXMODE_PROJECTION || this->_mtxCurrentMode == MATRIXMODE_TEXTURE)
	{
		//parameter ignored and treated as sensible
		if (this->_mtxCurrentMode == MATRIXMODE_PROJECTION)
		{
			MatrixCopy(this->_mtxCurrent[MATRIXMODE_PROJECTION], this->_mtxStackProjection[0]);
			this->UpdateMatrixProjectionLua();
		}
		else
		{
			MatrixCopy(this->_mtxCurrent[MATRIXMODE_TEXTURE], this->_mtxStackTexture[0]);
			this->_UpdateTransformedTexCoordsIfNeeded();
		}
	}
	else
	{
		//out of bounds errors function fully properly, but set errors
		MMU_new.gxstat.se = (param >= 31) ? 1 : 0; //(not sure, this might be off by 1)
		
		const u32 i = param & 0x0000001F;
		MatrixCopy(this->_mtxCurrent[MATRIXMODE_POSITION], this->_mtxStackPosition[i]);
		MatrixCopy(this->_mtxCurrent[MATRIXMODE_POSITION_VECTOR], this->_mtxStackPositionVector[i]);
	}
}

void NDSGeometryEngine::MatrixLoadIdentityToCurrent()
{
	MatrixIdentity(this->_mtxCurrent[this->_mtxCurrentMode]);
	
	if (this->_mtxCurrentMode == MATRIXMODE_TEXTURE)
	{
		this->_UpdateTransformedTexCoordsIfNeeded();
	}
	else if (this->_mtxCurrentMode == MATRIXMODE_POSITION_VECTOR)
	{
		MatrixIdentity(this->_mtxCurrent[MATRIXMODE_POSITION]);
	}

	//printf("identity: %d to: \n", this->_mtxCurrentMode); MatrixPrint(this->_mtxCurrent[1]);
}

void NDSGeometryEngine::MatrixLoad4x4()
{
	MatrixCopy(this->_mtxCurrent[this->_mtxCurrentMode], this->_pendingMtxLoad4x4);
	
	if (this->_mtxCurrentMode == MATRIXMODE_TEXTURE)
	{
		this->_UpdateTransformedTexCoordsIfNeeded();
	}
	else if (this->_mtxCurrentMode == MATRIXMODE_POSITION_VECTOR)
	{
		MatrixCopy(this->_mtxCurrent[MATRIXMODE_POSITION], this->_mtxCurrent[MATRIXMODE_POSITION_VECTOR]);
	}

	//printf("load4x4: matrix %d to: \n", this->_mtxCurrentMode); MatrixPrint(this->_mtxCurrent[1]);
}

void NDSGeometryEngine::MatrixLoad4x3()
{
	MatrixCopy(this->_mtxCurrent[this->_mtxCurrentMode], this->_pendingMtxLoad4x3);

	if (this->_mtxCurrentMode == MATRIXMODE_TEXTURE)
	{
		this->_UpdateTransformedTexCoordsIfNeeded();
	}
	else if (this->_mtxCurrentMode == MATRIXMODE_POSITION_VECTOR)
	{
		MatrixCopy(this->_mtxCurrent[MATRIXMODE_POSITION], this->_mtxCurrent[MATRIXMODE_POSITION_VECTOR]);
	}
	//printf("load4x3: matrix %d to: \n", this->_mtxCurrentMode); MatrixPrint(this->_mtxCurrent[1]);
}

void NDSGeometryEngine::MatrixMultiply4x4()
{
	MatrixMultiply(this->_mtxCurrent[this->_mtxCurrentMode], this->_tempMtxMultiply4x4);
	
	if (this->_mtxCurrentMode == MATRIXMODE_TEXTURE)
	{
		this->_UpdateTransformedTexCoordsIfNeeded();
	}
	else if (this->_mtxCurrentMode == MATRIXMODE_PROJECTION)
	{
		this->UpdateMatrixProjectionLua();
	}
	else if (this->_mtxCurrentMode == MATRIXMODE_POSITION_VECTOR)
	{
		MatrixMultiply(this->_mtxCurrent[MATRIXMODE_POSITION], this->_tempMtxMultiply4x4);
	}

	//printf("mult4x4: matrix %d to: \n", this->_mtxCurrentMode); MatrixPrint(this->_mtxCurrent[1]);
}

void NDSGeometryEngine::MatrixMultiply4x3()
{
	MatrixMultiply(this->_mtxCurrent[this->_mtxCurrentMode], this->_tempMtxMultiply4x3);
	
	if (this->_mtxCurrentMode == MATRIXMODE_TEXTURE)
	{
		this->_UpdateTransformedTexCoordsIfNeeded();
	}
	else if (this->_mtxCurrentMode == MATRIXMODE_PROJECTION)
	{
		this->UpdateMatrixProjectionLua();
	}
	else if (this->_mtxCurrentMode == MATRIXMODE_POSITION_VECTOR)
	{
		MatrixMultiply(this->_mtxCurrent[MATRIXMODE_POSITION], this->_tempMtxMultiply4x3);
	}

	//printf("mult4x3: matrix %d to: \n", this->_mtxCurrentMode); MatrixPrint(this->_mtxCurrent[1]);
}

void NDSGeometryEngine::MatrixMultiply3x3()
{
	MatrixMultiply(this->_mtxCurrent[this->_mtxCurrentMode], this->_tempMtxMultiply3x3);
	
	if (this->_mtxCurrentMode == MATRIXMODE_TEXTURE)
	{
		this->_UpdateTransformedTexCoordsIfNeeded();
	}
	else if (this->_mtxCurrentMode == MATRIXMODE_PROJECTION)
	{
		this->UpdateMatrixProjectionLua();
	}
	else if (this->_mtxCurrentMode == MATRIXMODE_POSITION_VECTOR)
	{
		MatrixMultiply(this->_mtxCurrent[MATRIXMODE_POSITION], this->_tempMtxMultiply3x3);
	}

	//printf("mult3x3: matrix %d to: \n", this->_mtxCurrentMode); MatrixPrint(this->_mtxCurrent[1]);
}

void NDSGeometryEngine::MatrixScale()
{
	::MatrixScale(this->_mtxCurrent[(this->_mtxCurrentMode == MATRIXMODE_POSITION_VECTOR ? MATRIXMODE_POSITION : this->_mtxCurrentMode)], this->_vecScale.vec);
	//printf("scale: matrix %d to: \n", this->_mtxCurrentMode); MatrixPrint(this->_mtxCurrent[1]);

	//note: pos-vector mode should not cause both matrices to scale.
	//the whole purpose is to keep the vector matrix orthogonal
	//so, I am leaving this commented out as an example of what not to do.
	//if (this->_mtxCurrentMode == MATRIXMODE_POSITION_VECTOR)
	//	::MatrixScale(this->_mtxCurrent[MATRIXMODE_POSITION], this->_vecScale.vec);
	
	if (this->_mtxCurrentMode == MATRIXMODE_TEXTURE)
	{
		this->_UpdateTransformedTexCoordsIfNeeded();
	}
}

void NDSGeometryEngine::MatrixTranslate()
{
	::MatrixTranslate(this->_mtxCurrent[this->_mtxCurrentMode], this->_vecTranslate.vec);
	
	if (this->_mtxCurrentMode == MATRIXMODE_TEXTURE)
	{
		this->_UpdateTransformedTexCoordsIfNeeded();
	}
	else if (this->_mtxCurrentMode == MATRIXMODE_POSITION_VECTOR)
	{
		::MatrixTranslate(this->_mtxCurrent[MATRIXMODE_POSITION], this->_vecTranslate.vec);
	}

	//printf("translate: matrix %d to: \n", this->_mtxCurrentMode); MatrixPrint(this->_mtxCurrent[1]);
}

// 0-4   Diffuse Reflection Red
// 5-9   Diffuse Reflection Green
// 10-14 Diffuse Reflection Blue
// 15    Set Vertex Color (0=No, 1=Set Diffuse Reflection Color as Vertex Color)
// 16-20 Ambient Reflection Red
// 21-25 Ambient Reflection Green
// 26-30 Ambient Reflection Blue
void NDSGeometryEngine::SetDiffuseAmbient(const u32 param)
{
	this->_regDiffuse = (u16)(param & 0x0000FFFF);
	this->_regAmbient = (u16)(param >> 16);

	if (BIT15(param))
	{
		this->SetVertexColor(param);
	}
}

void NDSGeometryEngine::SetSpecularEmission(const u32 param)
{
	this->_regSpecular = (u16)(param & 0x0000FFFF);
	this->_regEmission = (u16)(param >> 16);
}

// 0-9   Directional Vector's X component (1bit sign + 9bit fractional part)
// 10-19 Directional Vector's Y component (1bit sign + 9bit fractional part)
// 20-29 Directional Vector's Z component (1bit sign + 9bit fractional part)
// 30-31 Light Number                     (0...3)
void NDSGeometryEngine::SetLightDirection(const u32 param)
{
	const size_t index = (param >> 30) & 0x00000003;
	const u32 v = param & 0x3FFFFFFF;
	this->_regLightDirection[index] = param;
	
	// Unpack the vector, and then multiply it by the current directional matrix.
	this->_vecLightDirectionTransformed[index].x = ((s32)((v<<22) & 0xFFC00000) / (s32)(1<<22)) * (s32)(1<<3);
	this->_vecLightDirectionTransformed[index].y = ((s32)((v<<12) & 0xFFC00000) / (s32)(1<<22)) * (s32)(1<<3);
	this->_vecLightDirectionTransformed[index].z = ((s32)((v<< 2) & 0xFFC00000) / (s32)(1<<22)) * (s32)(1<<3);
	this->_vecLightDirectionTransformed[index].w = 0;
	MatrixMultVec3x3(this->_mtxCurrent[MATRIXMODE_POSITION_VECTOR], this->_vecLightDirectionTransformed[index].vec);
	
	this->_doesLightHalfVectorNeedUpdate[index] = true;
}

void NDSGeometryEngine::SetLightColor(const u32 param)
{
	const size_t index = param >> 30;
	this->_regLightColor[index] = param;
}

void NDSGeometryEngine::SetShininess(const u32 param)
{
	u32 &targetShininess = (u32 &)this->_shininessTablePending[this->_shininessTablePendingIndex];
	targetShininess = LE_TO_LOCAL_32(param);
	
	this->_shininessTablePendingIndex += 4;
	
	if (this->_shininessTablePendingIndex < 128)
	{
		return;
	}
	
	memcpy(this->_shininessTableApplied, this->_shininessTablePending, sizeof(this->_shininessTablePending));
	this->_shininessTablePendingIndex = 0;
}

void NDSGeometryEngine::SetNormal(const u32 param)
{
	this->_vecNormal.x = ((s32)((param << 22) & 0xFFC00000) / (s32)(1<<22)) * (s32)(1<<3);
	this->_vecNormal.y = ((s32)((param << 12) & 0xFFC00000) / (s32)(1<<22)) * (s32)(1<<3);
	this->_vecNormal.z = ((s32)((param <<  2) & 0xFFC00000) / (s32)(1<<22)) * (s32)(1<<3);
	this->_vecNormal.w =  (s32)(1 << 12);
	
	if (this->_texCoordTransformMode == TextureTransformationMode_NormalSource)
	{
		this->_doesTransformedTexCoordsNeedUpdate = true;
	}
	
	CACHE_ALIGN Vector4s32 normalTransformed = this->_vecNormal;
	MatrixMultVec3x3(_mtxCurrent[MATRIXMODE_POSITION_VECTOR], normalTransformed.vec);

	//apply lighting model
	const Color3s32 diffuse = {
		(s32)( this->_regDiffuse        & 0x001F),
		(s32)((this->_regDiffuse >>  5) & 0x001F),
		(s32)((this->_regDiffuse >> 10) & 0x001F)
	};

	const Color3s32 ambient = {
		(s32)( this->_regAmbient        & 0x001F),
		(s32)((this->_regAmbient >>  5) & 0x001F),
		(s32)((this->_regAmbient >> 10) & 0x001F)
	};

	const Color3s32 emission = {
		(s32)( this->_regEmission        & 0x001F),
		(s32)((this->_regEmission >>  5) & 0x001F),
		(s32)((this->_regEmission >> 10) & 0x001F)
	};

	const Color3s32 specular = {
		(s32)( this->_regSpecular        & 0x001F),
		(s32)((this->_regSpecular >>  5) & 0x001F),
		(s32)((this->_regSpecular >> 10) & 0x001F)
	};

	Color3s32 vertexColor = emission;

	const u8 lightMask = gfx3d.regPolyAttrApplied.LightMask;
	
	for (size_t i = 0; i < 4; i++)
	{
		if (!((lightMask >> i) & 1))
		{
			continue;
		}
		
		const Color3s32 lightColor = {
			(s32)( this->_regLightColor[i]        & 0x0000001F),
			(s32)((this->_regLightColor[i] >>  5) & 0x0000001F),
			(s32)((this->_regLightColor[i] >> 10) & 0x0000001F)
		};

		//This formula is the one used by the DS
		//Reference : http://nocash.emubase.de/gbatek.htm#ds3dpolygonlightparameters
		const s32 fixed_diffuse = std::max( 0, -vec3dot_fixed32(this->_vecLightDirectionTransformed[i].vec, normalTransformed.vec) );
		
		if (this->_doesLightHalfVectorNeedUpdate[i])
		{
			this->UpdateLightDirectionHalfAngleVector(i);
		}
		
		const s32 dot = vec3dot_fixed32(this->_vecLightDirectionHalfNegative[i].vec, normalTransformed.vec);

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
		
		if (this->_regSpecular & 0x8000)
		{
			//shininess is 20.12 fixed point, so >>5 gives us .7 which is 128 entries
			//the entries are 8bits each so <<4 gives us .12 again, compatible with the lighting formulas below
			//(according to other normal nds procedures, we might should fill the bottom bits with 1 or 0 according to rules...)
			fixedshininess = this->_shininessTableApplied[fixedshininess>>5] << 4;
		}

		for (size_t c = 0; c < 3; c++)
		{
			const s32 specComp = ((specular.component[c] * lightColor.component[c] * fixedshininess) >> 17); // 5 bits for color*color and 12 bits for shininess
			const s32 diffComp = (( diffuse.component[c] * lightColor.component[c] * fixed_diffuse)  >> 17); // 5 bits for color*color and 12 bits for diffuse
			const s32 ambComp  = (( ambient.component[c] * lightColor.component[c]) >> 5); // 5 bits for color*color
			vertexColor.component[c] += specComp + diffComp + ambComp;
		}
	}
	
	const Color4u8 newVtxColor = {
		(u8)std::min<s32>(31, vertexColor.r),
		(u8)std::min<s32>(31, vertexColor.g),
		(u8)std::min<s32>(31, vertexColor.b),
		0
	};
	
	this->SetVertexColor(newVtxColor);
}

void NDSGeometryEngine::SetViewport(const u32 param)
{
	if (this->_regViewport.value != param)
	{
		this->_regViewport.value = param;
		this->_doesViewportNeedUpdate = true;
	}
}

void NDSGeometryEngine::SetViewport(const IOREG_VIEWPORT regViewport)
{
	if (this->_regViewport.value != regViewport.value)
	{
		this->_regViewport = regViewport;
		this->_doesViewportNeedUpdate = true;
	}
}

void NDSGeometryEngine::SetViewport(const GFX3D_Viewport viewport)
{
	this->_regViewport = _GFX3D_IORegisterMap->VIEWPORT;
	this->_currentViewport = viewport;
	this->_doesViewportNeedUpdate = false;
}

void NDSGeometryEngine::SetVertexColor(const u32 param)
{
	const u32 param15 = (param & 0x00007FFF);
	if (this->_vtxColor15 != param15)
	{
		this->_vtxColor15 = param15;
		this->_vtxColor555X.r = (u8)((param >>  0) & 0x0000001F);
		this->_vtxColor555X.g = (u8)((param >>  5) & 0x0000001F);
		this->_vtxColor555X.b = (u8)((param >> 10) & 0x0000001F);
		this->_doesVertexColorNeedUpdate = true;
	}
}

void NDSGeometryEngine::SetVertexColor(const Color4u8 vtxColor555X)
{
	if (this->_vtxColor555X.value != vtxColor555X.value)
	{
		this->_vtxColor15 = (vtxColor555X.r << 0) | (vtxColor555X.g << 5) | (vtxColor555X.b << 10);
		this->_vtxColor555X = vtxColor555X;
		this->_doesVertexColorNeedUpdate = true;
	}
}

void NDSGeometryEngine::SetTextureParameters(const u32 param)
{
	const TEXIMAGE_PARAM newTexParam = { param };
	this->SetTextureParameters(newTexParam);
}

void NDSGeometryEngine::SetTextureParameters(const TEXIMAGE_PARAM texParams)
{
	this->_texParam = texParams;
	
	if (this->_texCoordTransformMode != texParams.TexCoordTransformMode)
	{
		this->_texCoordTransformMode = (TextureTransformationMode)texParams.TexCoordTransformMode;
		this->_doesTransformedTexCoordsNeedUpdate = true;
	}
}

void NDSGeometryEngine::SetTexturePalette(const u32 texPalette)
{
	this->_texPalette = texPalette;
}

void NDSGeometryEngine::SetTextureCoordinates2s16(const u32 param)
{
	Vector2s16 inTexCoord2s16;
	inTexCoord2s16.value = LE_TO_LOCAL_WORDS_32(param);
	
	this->SetTextureCoordinates2s16(inTexCoord2s16);
}

void NDSGeometryEngine::SetTextureCoordinates2s16(const Vector2s16 &texCoord16)
{
	if (this->_texCoord16.value != texCoord16.value)
	{
		this->_texCoord16 = texCoord16;
		this->_doesTransformedTexCoordsNeedUpdate = true;
	}
}

void NDSGeometryEngine::VertexListBegin(const u32 param, const POLYGON_ATTR polyAttr)
{
	const PolygonPrimitiveType vtxFormat = (PolygonPrimitiveType)(param & 0x00000003);
	this->VertexListBegin(vtxFormat, polyAttr);
}

void NDSGeometryEngine::VertexListBegin(const PolygonPrimitiveType vtxFormat, const POLYGON_ATTR polyAttr)
{
	this->_inBegin = true;
	
	this->_polyAttribute = polyAttr;
	this->_vtxFormat = vtxFormat;
	this->_vtxCount = 0;
	this->_vtxIndex[0] = 0;
	this->_vtxIndex[1] = 0;
	this->_vtxIndex[2] = 0;
	this->_vtxIndex[3] = 0;
	this->_isGeneratingFirstPolyOfStrip = true;
	this->_generateTriangleStripIndexToggle = false;
}

void NDSGeometryEngine::VertexListEnd()
{
	this->_inBegin = false;
	this->_vtxCount = 0;
}

bool NDSGeometryEngine::SetCurrentVertexPosition2s16(const u32 param)
{
	Vector2s16 inVtxCoord2s16;
	inVtxCoord2s16.value = LE_TO_LOCAL_WORDS_32(param);
	
	return this->SetCurrentVertexPosition2s16(inVtxCoord2s16);
}

bool NDSGeometryEngine::SetCurrentVertexPosition2s16(const Vector2s16 inVtxCoord2s16)
{
	if (this->_vtxCoord16CurrentIndex == 0)
	{
		this->SetCurrentVertexPosition2s16Immediate<0, 1>(inVtxCoord2s16);
		this->_vtxCoord16CurrentIndex++;
		return false;
	}
	
	this->SetCurrentVertexPosition2s16Immediate<2, 3>(inVtxCoord2s16);
	this->_vtxCoord16CurrentIndex = 0;
	return true;
}

void NDSGeometryEngine::SetCurrentVertexPosition3s10(const u32 param)
{
	const Vector3s16 inVtxCoord3s16 = {
		(s16)( ((s32)((param << 22) & 0xFFC00000) / (s32)(1 << 22)) * (s32)(1 << 6) ),
		(s16)( ((s32)((param << 12) & 0xFFC00000) / (s32)(1 << 22)) * (s32)(1 << 6) ),
		(s16)( ((s32)((param <<  2) & 0xFFC00000) / (s32)(1 << 22)) * (s32)(1 << 6) )
	};
	
	this->SetCurrentVertexPosition(inVtxCoord3s16);
}

void NDSGeometryEngine::SetCurrentVertexPosition(const Vector3s16 inVtxCoord3s16)
{
	this->_vtxCoord16 = inVtxCoord3s16;
}

template <size_t ONE, size_t TWO>
void NDSGeometryEngine::SetCurrentVertexPosition2s16Immediate(const u32 param)
{
	Vector2s16 inVtxCoord2s16;
	inVtxCoord2s16.value = LE_TO_LOCAL_WORDS_32(param);
	
	this->SetCurrentVertexPosition2s16Immediate<ONE, TWO>(inVtxCoord2s16);
}

template <size_t ONE, size_t TWO>
void NDSGeometryEngine::SetCurrentVertexPosition2s16Immediate(const Vector2s16 inVtxCoord2s16)
{
	if (ONE < 3)
	{
		this->_vtxCoord16.coord[ONE] = inVtxCoord2s16.coord[0];
	}
	
	if (TWO < 3)
	{
		this->_vtxCoord16.coord[TWO] = inVtxCoord2s16.coord[1];
	}
}

void NDSGeometryEngine::SetCurrentVertexPosition3s10Relative(const u32 param)
{
	const Vector3s16 inVtxCoord3s16 = {
		(s16)( (s32)((param << 22) & 0xFFC00000) / (s32)(1 << 22) ),
		(s16)( (s32)((param << 12) & 0xFFC00000) / (s32)(1 << 22) ),
		(s16)( (s32)((param <<  2) & 0xFFC00000) / (s32)(1 << 22) )
	};
	
	this->SetCurrentVertexPositionRelative(inVtxCoord3s16);
}

void NDSGeometryEngine::SetCurrentVertexPositionRelative(const Vector3s16 inVtxCoord3s16)
{
	this->_vtxCoord16.x += inVtxCoord3s16.x;
	this->_vtxCoord16.y += inVtxCoord3s16.y;
	this->_vtxCoord16.z += inVtxCoord3s16.z;
}

//Submit a vertex to the GE
void NDSGeometryEngine::AddCurrentVertexToList(GFX3D_GeometryList &targetGList)
{
	//refuse to do anything if we have too many verts or polys
	if (targetGList.rawVertCount >= VERTLIST_SIZE)
	{
		return;
	}
	
	if (targetGList.rawPolyCount >= POLYLIST_SIZE)
	{
		return;
	}
	
	CACHE_ALIGN Vector4s32 vtxCoordTransformed = {
		(s32)this->_vtxCoord16.x,
		(s32)this->_vtxCoord16.y,
		(s32)this->_vtxCoord16.z,
		(s32)(1<<12)
	};

	// Perform the vertex coordinate transformation.
	if (freelookMode == 2)
	{
		//adjust projection
		s32 tmp[16];
		MatrixCopy(tmp, this->_mtxCurrent[MATRIXMODE_PROJECTION]);
		MatrixMultiply(tmp, freelookMatrix);
		GEM_TransformVertex(this->_mtxCurrent[MATRIXMODE_POSITION], vtxCoordTransformed.coord); //modelview
		GEM_TransformVertex(tmp, vtxCoordTransformed.coord); //projection
	}
	else if (freelookMode == 3)
	{
		//use provided projection
		GEM_TransformVertex(this->_mtxCurrent[MATRIXMODE_POSITION], vtxCoordTransformed.coord); //modelview
		GEM_TransformVertex(freelookMatrix, vtxCoordTransformed.coord); //projection
	}
	else
	{
		//no freelook
		GEM_TransformVertex(this->_mtxCurrent[MATRIXMODE_POSITION], vtxCoordTransformed.coord); //modelview
		GEM_TransformVertex(this->_mtxCurrent[MATRIXMODE_PROJECTION], vtxCoordTransformed.coord); //projection
	}

	// TODO: Culling should be done here.
	// TODO: Perform viewport transform here?
	
	// Perform the vertex color conversion if needed.
	if (this->_doesVertexColorNeedUpdate)
	{
		this->_vtxColor666X.r = GFX3D_5TO6_LOOKUP(this->_vtxColor555X.r);
		this->_vtxColor666X.g = GFX3D_5TO6_LOOKUP(this->_vtxColor555X.g);
		this->_vtxColor666X.b = GFX3D_5TO6_LOOKUP(this->_vtxColor555X.b);
		this->_vtxColor666X.a = 0;
		
		this->_doesVertexColorNeedUpdate = false;
	}
	
	// Perform the texture coordinate transformation if needed.
	if ( this->_doesTransformedTexCoordsNeedUpdate ||
	    (this->_texParam.TexCoordTransformMode == TextureTransformationMode_VertexSource) )
	{
		const NDSMatrix &mtxTexture = this->_mtxCurrent[MATRIXMODE_TEXTURE];
		
		switch (this->_texCoordTransformMode)
		{
			//dragon quest 4 overworld will test this
			case TextureTransformationMode_TexCoordSource:
				this->_texCoordTransformed.s = (s32)( (s64)(GEM_Mul32x16To64(mtxTexture[0], this->_texCoord16.s) + GEM_Mul32x16To64(mtxTexture[4], this->_texCoord16.t) + (s64)mtxTexture[8] + (s64)mtxTexture[12]) >> 12 );
				this->_texCoordTransformed.t = (s32)( (s64)(GEM_Mul32x16To64(mtxTexture[1], this->_texCoord16.s) + GEM_Mul32x16To64(mtxTexture[5], this->_texCoord16.t) + (s64)mtxTexture[9] + (s64)mtxTexture[13]) >> 12 );
				break;
				
			//SM64 highlight rendered star in main menu tests this
			//also smackdown 2010 player textures tested this (needed cast on _s and _t)
			case TextureTransformationMode_NormalSource:
				this->_texCoordTransformed.s = (s32)( (s64)(GEM_Mul32x32To64(mtxTexture[0], this->_vecNormal.x) + GEM_Mul32x32To64(mtxTexture[4], this->_vecNormal.y) + GEM_Mul32x32To64(mtxTexture[8], this->_vecNormal.z) + ((s64)this->_texCoord16.s << 24)) >> 24 );
				this->_texCoordTransformed.t = (s32)( (s64)(GEM_Mul32x32To64(mtxTexture[1], this->_vecNormal.x) + GEM_Mul32x32To64(mtxTexture[5], this->_vecNormal.y) + GEM_Mul32x32To64(mtxTexture[9], this->_vecNormal.z) + ((s64)this->_texCoord16.t << 24)) >> 24 );
				break;
				
			//Tested by: Eledees The Adventures of Kai and Zero (E) [title screen and frontend menus]
			case TextureTransformationMode_VertexSource:
				this->_texCoordTransformed.s = (s32)( (s64)(GEM_Mul32x16To64(mtxTexture[0], this->_vtxCoord16.x) + GEM_Mul32x16To64(mtxTexture[4], this->_vtxCoord16.y) + GEM_Mul32x16To64(mtxTexture[8], this->_vtxCoord16.z) + ((s64)this->_texCoord16.s << 24)) >> 24 );
				this->_texCoordTransformed.t = (s32)( (s64)(GEM_Mul32x16To64(mtxTexture[1], this->_vtxCoord16.x) + GEM_Mul32x16To64(mtxTexture[5], this->_vtxCoord16.y) + GEM_Mul32x16To64(mtxTexture[9], this->_vtxCoord16.z) + ((s64)this->_texCoord16.t << 24)) >> 24 );
				break;
				
			default: // TextureTransformationMode_None
				this->_texCoordTransformed.s = (s32)this->_texCoord16.s;
				this->_texCoordTransformed.t = (s32)this->_texCoord16.t;
				break;
		}
		
		this->_doesTransformedTexCoordsNeedUpdate = false;
	}
	
	size_t continuation = 0;
	if ( !this->_isGeneratingFirstPolyOfStrip &&
	    ((this->_vtxFormat == GFX3D_TRIANGLE_STRIP) || (this->_vtxFormat == GFX3D_QUAD_STRIP)) )
	{
		continuation = 2;
	}

	//record the vertex
	const size_t vertIndex = targetGList.rawVertCount + this->_vtxCount - continuation;
	if (vertIndex >= VERTLIST_SIZE)
	{
		printf("wtf\n");
	}
	
	NDSVertex &vtx = targetGList.rawVtxList[vertIndex];
	vtx.position = vtxCoordTransformed;
	vtx.texCoord = this->_texCoordTransformed;
	vtx.color    = this->_vtxColor666X;
	
	this->_vtxIndex[this->_vtxCount] = (u16)(targetGList.rawVertCount + this->_vtxCount - continuation);
	this->_vtxCount++;

	//possibly complete a polygon
	POLY &poly = targetGList.rawPolyList[targetGList.rawPolyCount];
	bool isCompletedPoly = false;
	
	switch (this->_vtxFormat)
	{
		case GFX3D_TRIANGLES:
		{
			if (this->_vtxCount != 3)
			{
				break;
			}
			
			isCompletedPoly = true;
			poly.type = POLYGON_TYPE_TRIANGLE;
			poly.vertIndexes[0] = this->_vtxIndex[0];
			poly.vertIndexes[1] = this->_vtxIndex[1];
			poly.vertIndexes[2] = this->_vtxIndex[2];
			poly.vertIndexes[3] = 0;
			
			targetGList.rawVertCount += 3;
			this->_vtxCount = 0;
			break;
		}
			
		case GFX3D_QUADS:
		{
			if (this->_vtxCount != 4)
			{
				break;
			}
			
			isCompletedPoly = true;
			poly.type = POLYGON_TYPE_QUAD;
			poly.vertIndexes[0] = this->_vtxIndex[0];
			poly.vertIndexes[1] = this->_vtxIndex[1];
			poly.vertIndexes[2] = this->_vtxIndex[2];
			poly.vertIndexes[3] = this->_vtxIndex[3];
			
			targetGList.rawVertCount += 4;
			this->_vtxCount = 0;
			break;
		}
			
		case GFX3D_TRIANGLE_STRIP:
		{
			if (this->_vtxCount != 3)
			{
				break;
			}
			
			isCompletedPoly = true;
			poly.type = POLYGON_TYPE_TRIANGLE;
			poly.vertIndexes[0] = this->_vtxIndex[0];
			poly.vertIndexes[1] = this->_vtxIndex[1];
			poly.vertIndexes[2] = this->_vtxIndex[2];
			poly.vertIndexes[3] = 0;
			
			if (this->_generateTriangleStripIndexToggle)
			{
				this->_vtxIndex[1] = (u16)(targetGList.rawVertCount + 2 - continuation);
			}
			else
			{
				this->_vtxIndex[0] = (u16)(targetGList.rawVertCount + 2 - continuation);
			}
			
			if (this->_isGeneratingFirstPolyOfStrip)
			{
				targetGList.rawVertCount += 3;
			}
			else
			{
				targetGList.rawVertCount += 1;
			}
			
			this->_generateTriangleStripIndexToggle = !this->_generateTriangleStripIndexToggle;
			this->_isGeneratingFirstPolyOfStrip = false;
			this->_vtxCount = 2;
			break;
		}
			
		case GFX3D_QUAD_STRIP:
		{
			if (this->_vtxCount != 4)
			{
				break;
			}
			
			isCompletedPoly = true;
			poly.type = POLYGON_TYPE_QUAD;
			poly.vertIndexes[0] = this->_vtxIndex[0];
			poly.vertIndexes[1] = this->_vtxIndex[1];
			poly.vertIndexes[2] = this->_vtxIndex[3]; // Note that the vertex index differs here.
			poly.vertIndexes[3] = this->_vtxIndex[2]; // Note that the vertex index differs here.
			
			this->_vtxIndex[0] = (u16)(targetGList.rawVertCount + 2 - continuation);
			this->_vtxIndex[1] = (u16)(targetGList.rawVertCount + 3 - continuation);
			
			if (this->_isGeneratingFirstPolyOfStrip)
			{
				targetGList.rawVertCount += 4;
			}
			else
			{
				targetGList.rawVertCount += 2;
			}
			
			this->_isGeneratingFirstPolyOfStrip = false;
			this->_vtxCount = 2;
			break;
		}
			
		default:
			return;
	}
	
	if (isCompletedPoly)
	{
		this->GeneratePolygon(poly, targetGList);
	}
}

void NDSGeometryEngine::GeneratePolygon(POLY &targetPoly, GFX3D_GeometryList &targetGList)
{
	targetPoly.vtxFormat = this->_vtxFormat;

	// Line segment detect
	// Tested" Castlevania POR - warp stone, trajectory of ricochet, "Eye of Decay"
	if (this->_texParam.PackedFormat == TEXMODE_NONE)
	{
		const NDSVertex &vtx0 = targetGList.rawVtxList[targetPoly.vertIndexes[0]];
		const NDSVertex &vtx1 = targetGList.rawVtxList[targetPoly.vertIndexes[1]];
		const NDSVertex &vtx2 = targetGList.rawVtxList[targetPoly.vertIndexes[2]];
		
		if ( ((vtx0.position.x == vtx1.position.x) && (vtx0.position.y == vtx1.position.y)) ||
		     ((vtx1.position.x == vtx2.position.x) && (vtx1.position.y == vtx2.position.y)) ||
		     ((vtx0.position.y == vtx1.position.y) && (vtx1.position.y == vtx2.position.y)) ||
		     ((vtx0.position.x == vtx1.position.x) && (vtx1.position.x == vtx2.position.x)) )
		{
			//printf("Line Segment detected (poly type %i, mode %i, texparam %08X)\n", poly.type, poly.vtxFormat, textureFormat);
			targetPoly.vtxFormat = (PolygonPrimitiveType)(this->_vtxFormat + 4);
		}
	}
	
	if (this->_doesViewportNeedUpdate)
	{
		this->_currentViewport = GFX3D_ViewportParse(this->_regViewport);
	}
	
	targetPoly.attribute  = this->_polyAttribute;
	targetPoly.texParam   = this->_texParam;
	targetPoly.texPalette = this->_texPalette;
	targetPoly.viewport   = this->_currentViewport;
	gfx3d.legacySave.rawPolyViewport[targetGList.rawPolyCount] = _GFX3D_IORegisterMap->VIEWPORT;
	
	targetGList.rawPolyCount++;
}

bool NDSGeometryEngine::SetCurrentBoxTestCoords(const u32 param)
{
	//clear result flag. busy flag has been set by fifo component already
	MMU_new.gxstat.tr = 0;

	this->_boxTestCoord16[this->_boxTestCoordCurrentIndex++] = (u16)(param & 0x0000FFFF);
	this->_boxTestCoord16[this->_boxTestCoordCurrentIndex++] = (u16)(param >> 16);

	if (this->_boxTestCoordCurrentIndex < 5)
	{
		return false;
	}
	
	this->_boxTestCoordCurrentIndex = 0;
	return true;
}

void NDSGeometryEngine::BoxTest()
{
	//now that we're executing this, we're not busy anymore
	MMU_new.gxstat.tb = 0;

	//(crafted to be clear, not fast.)
	//nanostray title, ff4, ice age 3 depend on this and work
	//garfields nightmare and strawberry shortcake DO DEPEND on the overflow behavior.

	const u16 ux = this->_boxTestCoord16[0];
	const u16 uy = this->_boxTestCoord16[1];
	const u16 uz = this->_boxTestCoord16[2];
	const u16 uw = this->_boxTestCoord16[3];
	const u16 uh = this->_boxTestCoord16[4];
	const u16 ud = this->_boxTestCoord16[5];

	//craft the coords by adding extents to startpoint
	const s32 fixedOne = 1 << 12;
	const s32 __x = (s32)((s16)ux);
	const s32 __y = (s32)((s16)uy);
	const s32 __z = (s32)((s16)uz);
	const s32 x_w = (s32)( (s16)((ux+uw) & 0xFFFF) );
	const s32 y_h = (s32)( (s16)((uy+uh) & 0xFFFF) );
	const s32 z_d = (s32)( (s16)((uz+ud) & 0xFFFF) );
	
	//eight corners of cube
	CACHE_ALIGN Vector4s32 vtxPosition[8] = {
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
	
	CACHE_ALIGN NDSVertex tempRawVtx[8];

	//transform all coords
	for (size_t i = 0; i < 8; i++)
	{
		MatrixMultVec4x4(this->_mtxCurrent[MATRIXMODE_POSITION], vtxPosition[i].coord);
		MatrixMultVec4x4(this->_mtxCurrent[MATRIXMODE_PROJECTION], vtxPosition[i].coord);
		
		tempRawVtx[i].position = vtxPosition[i];
	}

	//clip each poly
	for (size_t i = 0; i < 6; i++)
	{
		const POLY &rawPoly = tempRawPoly[i];
		const NDSVertex *rawPolyVtx[4] = {
			&tempRawVtx[rawPoly.vertIndexes[0]],
			&tempRawVtx[rawPoly.vertIndexes[1]],
			&tempRawVtx[rawPoly.vertIndexes[2]],
			&tempRawVtx[rawPoly.vertIndexes[3]]
		};
		
		const PolygonType cpType = GFX3D_GenerateClippedPoly<ClipperMode_DetermineClipOnly>(0, rawPoly.type, rawPolyVtx, tempClippedPoly);
		
		//if any portion of this poly was retained, then the test passes.
		if (cpType != POLYGON_TYPE_UNDEFINED)
		{
			MMU_new.gxstat.tr = 1;
			break;
		}
	}
}

bool NDSGeometryEngine::SetCurrentPositionTestCoords(const u32 param)
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
	this->_positionTestVtx32.coord[this->_positionTestCoordCurrentIndex++] = (s32)( (s16)((u16)(param & 0x0000FFFF)) );
	this->_positionTestVtx32.coord[this->_positionTestCoordCurrentIndex++] = (s32)( (s16)((u16)(param >> 16)) );

	if (this->_positionTestCoordCurrentIndex < 3)
	{
		return false;
	}
	
	this->_positionTestCoordCurrentIndex = 0;
	this->_positionTestVtx32.coord[3] = (s32)(1 << 12);
	
	return true;
}

void NDSGeometryEngine::PositionTest()
{
	MatrixMultVec4x4(this->_mtxCurrent[MATRIXMODE_POSITION], this->_positionTestVtx32.coord);
	MatrixMultVec4x4(this->_mtxCurrent[MATRIXMODE_PROJECTION], this->_positionTestVtx32.coord);
	
	MMU_new.gxstat.tb = 0;
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x620, (u32)this->_positionTestVtx32.coord[0]);
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x624, (u32)this->_positionTestVtx32.coord[1]);
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x628, (u32)this->_positionTestVtx32.coord[2]);
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x62C, (u32)this->_positionTestVtx32.coord[3]);
}

void NDSGeometryEngine::VectorTest(const u32 param)
{
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
	CACHE_ALIGN Vector4s32 testVec = {
		( (s32)((param << 22) & 0xFFC00000) / (s32)(1 << 19) ) | (s32)((param & 0x000001C0) >>  6),
		( (s32)((param << 12) & 0xFFC00000) / (s32)(1 << 19) ) | (s32)((param & 0x00007000) >> 16),
		( (s32)((param <<  2) & 0xFFC00000) / (s32)(1 << 19) ) | (s32)((param & 0x01C00000) >> 26),
		0
	};
	
	MatrixMultVec4x4(this->_mtxCurrent[MATRIXMODE_POSITION_VECTOR], testVec.vec);
	
	// The result vector is 4.12 fixed-point, but the upper 4 bits are all sign bits with no
	// integer part. There is also an overflow and sign-expansion behavior that occurs for values
	// greater than or equal to 1.0f (or 4096 in fixed-point). All of this means that for all
	// values >= 1.0f or < -1.0f will result in the sign bits becoming 1111b; otherwise, the sign
	// bits will become 0000b.
	const Vector3s16 resultVec = {
		(s16)( ((testVec.x > 0) && (testVec.x < 4096)) ? ((s16)testVec.x & 0x0FFF) : ((s16)testVec.x | 0xF000) ),
		(s16)( ((testVec.y > 0) && (testVec.y < 4096)) ? ((s16)testVec.y & 0x0FFF) : ((s16)testVec.y | 0xF000) ),
		(s16)( ((testVec.z > 0) && (testVec.z < 4096)) ? ((s16)testVec.z & 0x0FFF) : ((s16)testVec.z | 0xF000) )
	};
	
	MMU_new.gxstat.tb = 0; // clear busy
	T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x630, (u16)resultVec.x);
	T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x632, (u16)resultVec.y);
	T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x634, (u16)resultVec.z);
}

void NDSGeometryEngine::MatrixCopyFromCurrent(const MatrixMode whichMatrix, NDSMatrixFloat &outMtx)
{
	MatrixCopy(outMtx, this->_mtxCurrent[whichMatrix]);
}

void NDSGeometryEngine::MatrixCopyFromCurrent(const MatrixMode whichMatrix, NDSMatrix &outMtx)
{
	MatrixCopy(outMtx, this->_mtxCurrent[whichMatrix]);
}

void NDSGeometryEngine::MatrixCopyFromStack(const MatrixMode whichMatrix, const size_t stackIndex, NDSMatrixFloat &outMtx)
{
	if (stackIndex > 31)
	{
		return;
	}
	
	switch (whichMatrix)
	{
		case MATRIXMODE_PROJECTION:
			MatrixCopy(outMtx, this->_mtxStackProjection[0]);
			break;
			
		case MATRIXMODE_POSITION:
			MatrixCopy(outMtx, this->_mtxStackPosition[stackIndex]);
			break;
			
		case MATRIXMODE_POSITION_VECTOR:
			MatrixCopy(outMtx, this->_mtxStackPositionVector[stackIndex]);
			break;
			
		case MATRIXMODE_TEXTURE:
			MatrixCopy(outMtx, this->_mtxStackTexture[0]);
			break;
			
		default:
			break;
	}
}

void NDSGeometryEngine::MatrixCopyFromStack(const MatrixMode whichMatrix, const size_t stackIndex, NDSMatrix &outMtx)
{
	if (stackIndex > 31)
	{
		return;
	}
	
	switch (whichMatrix)
	{
		case MATRIXMODE_PROJECTION:
			MatrixCopy(outMtx, this->_mtxStackProjection[0]);
			break;
			
		case MATRIXMODE_POSITION:
			MatrixCopy(outMtx, this->_mtxStackPosition[stackIndex]);
			break;
			
		case MATRIXMODE_POSITION_VECTOR:
			MatrixCopy(outMtx, this->_mtxStackPositionVector[stackIndex]);
			break;
			
		case MATRIXMODE_TEXTURE:
			MatrixCopy(outMtx, this->_mtxStackTexture[0]);
			break;
			
		default:
			break;
	}
}

void NDSGeometryEngine::MatrixCopyToStack(const MatrixMode whichMatrix, const size_t stackIndex, const NDSMatrix &inMtx)
{
	if (stackIndex > 31)
	{
		return;
	}
	
	switch (whichMatrix)
	{
		case MATRIXMODE_PROJECTION:
			MatrixCopy(this->_mtxStackProjection[0], inMtx);
			break;
			
		case MATRIXMODE_POSITION:
			MatrixCopy(this->_mtxStackPosition[stackIndex], inMtx);
			break;
			
		case MATRIXMODE_POSITION_VECTOR:
			MatrixCopy(this->_mtxStackPositionVector[stackIndex], inMtx);
			break;
			
		case MATRIXMODE_TEXTURE:
			MatrixCopy(this->_mtxStackTexture[0], inMtx);
			break;
			
		default:
			break;
	}
}

void NDSGeometryEngine::UpdateLightDirectionHalfAngleVector(const size_t index)
{
	static const CACHE_ALIGN Vector4s32 lineOfSight = {0, 0, (s32)0xFFFFF000, 0};
	
	Vector4s32 half = {
		this->_vecLightDirectionTransformed[index].x + lineOfSight.x,
		this->_vecLightDirectionTransformed[index].y + lineOfSight.y,
		this->_vecLightDirectionTransformed[index].z + lineOfSight.z,
		this->_vecLightDirectionTransformed[index].w + lineOfSight.w
	};
	
	//normalize the half angle vector
	//can't believe the hardware really does this... but yet it seems...
	s32 halfLength = ( (s32)(sqrt((double)vec3dot_fixed32(half.vec, half.vec))) ) << 6;

	if (halfLength != 0)
	{
		halfLength = abs(halfLength);
		halfLength >>= 6;
		for (size_t i = 0; i < 4; i++)
		{
			s32 temp = half.vec[i];
			temp <<= 6;
			temp /= halfLength;
			half.vec[i] = temp;
		}
	}
	
	this->_vecLightDirectionHalfNegative[index].x = -half.x;
	this->_vecLightDirectionHalfNegative[index].y = -half.y;
	this->_vecLightDirectionHalfNegative[index].z = -half.z;
	this->_vecLightDirectionHalfNegative[index].w = -half.w;
	this->_doesLightHalfVectorNeedUpdate[index] = false;
}

void NDSGeometryEngine::UpdateMatrixProjectionLua()
{
#ifdef HAVE_LUA
	if (freelookMode == 0)
	{
		return;
	}
	
	float floatproj[16];
	MatrixCopy(floatproj, this->_mtxCurrent[MATRIXMODE_PROJECTION]);
	
	CallRegistered3dEvent(0, floatproj);
#endif
}

u32 NDSGeometryEngine::GetClipMatrixAtIndex(const u32 requestedIndex) const
{
	return (u32)MatrixGetMultipliedIndex(requestedIndex, this->_mtxCurrent[MATRIXMODE_PROJECTION], this->_mtxCurrent[MATRIXMODE_POSITION]);
}

u32 NDSGeometryEngine::GetDirectionalMatrixAtIndex(const u32 requestedIndex) const
{
	// When reading the current directional matrix, the read indices are sent
	// sequentially, with values [0...8], assuming a 3x3 matrix. However, the
	// directional matrix is stored as a 4x4 matrix with 16 values [0...15]. This
	// means that in order to read the correct element from the directional matrix,
	// the requested index needs to be mapped to the actual element index of the
	// matrix.
	
	const size_t elementIndex = (((requestedIndex / 3) * 4) + (requestedIndex % 3));
	return (u32)this->_mtxCurrent[MATRIXMODE_POSITION_VECTOR][elementIndex];
}

u32 NDSGeometryEngine::GetPositionTestResult(const u32 requestedIndex) const
{
	return (u32)_positionTestVtx32.coord[requestedIndex];
}

void NDSGeometryEngine::SaveState_LegacyFormat(GeometryEngineLegacySave &outLegacySave)
{
	outLegacySave.inBegin = (this->_inBegin) ? TRUE : FALSE;
	outLegacySave.texParam = this->_texParam;
	outLegacySave.texPalette = this->_texPalette;
	
	outLegacySave.mtxCurrentMode = (u32)this->_mtxCurrentMode;
	
	// Historically, only the last multiply matrix used is the one that is saved.
	switch (this->_lastMtxMultCommand)
	{
		case LastMtxMultCommand_4x3:
			MatrixCopy(outLegacySave.tempMultiplyMatrix, this->_tempMtxMultiply4x3);
			break;
			
		case LastMtxMultCommand_3x3:
			MatrixCopy(outLegacySave.tempMultiplyMatrix, this->_tempMtxMultiply3x3);
			break;
			
		default: // LastMtxMultCommand_4x4
			MatrixCopy(outLegacySave.tempMultiplyMatrix, this->_tempMtxMultiply4x4);
			break;
	}
	
	MatrixCopy(outLegacySave.currentMatrix[MATRIXMODE_PROJECTION],      this->_mtxCurrent[MATRIXMODE_PROJECTION]);
	MatrixCopy(outLegacySave.currentMatrix[MATRIXMODE_POSITION],        this->_mtxCurrent[MATRIXMODE_POSITION]);
	MatrixCopy(outLegacySave.currentMatrix[MATRIXMODE_POSITION_VECTOR], this->_mtxCurrent[MATRIXMODE_POSITION_VECTOR]);
	MatrixCopy(outLegacySave.currentMatrix[MATRIXMODE_TEXTURE],         this->_mtxCurrent[MATRIXMODE_TEXTURE]);
	
	outLegacySave.mtxLoad4x4PendingIndex = this->_mtxLoad4x4PendingIndex;
	outLegacySave.mtxLoad4x3PendingIndex = this->_mtxLoad4x3PendingIndex;
	outLegacySave.mtxMultiply4x4TempIndex = this->_mtxMultiply4x4TempIndex;
	outLegacySave.mtxMultiply4x3TempIndex = this->_mtxMultiply4x3TempIndex;
	outLegacySave.mtxMultiply3x3TempIndex = this->_mtxMultiply3x3TempIndex;
	
	outLegacySave.vtxPosition.vec3 = this->_vtxCoord16;
	outLegacySave.vtxPosition.coord[3] = 0;
	outLegacySave.vtxPosition16CurrentIndex = this->_vtxCoord16CurrentIndex;
	outLegacySave.vtxFormat = (u32)this->_vtxFormat;
	
	outLegacySave.vecTranslate = this->_vecTranslate;
	outLegacySave.vecTranslateCurrentIndex = this->_vecTranslateCurrentIndex;
	outLegacySave.vecScale = this->_vecScale;
	outLegacySave.vecScaleCurrentIndex = this->_vecScaleCurrentIndex;
	
	outLegacySave.texCoordT = (u32)((u16)this->_texCoord16.t);
	outLegacySave.texCoordS = (u32)((u16)this->_texCoord16.s);
	outLegacySave.texCoordTransformedT = (u32)this->_texCoordTransformed.t;
	outLegacySave.texCoordTransformedS = (u32)this->_texCoordTransformed.s;
	
	outLegacySave.boxTestCoordCurrentIndex = (u32)this->_boxTestCoordCurrentIndex;
	outLegacySave.positionTestCoordCurrentIndex = (u32)this->_positionTestCoordCurrentIndex;
	outLegacySave.positionTestVtxFloat[0] = (float)this->_positionTestVtx32.x / 4096.f;
	outLegacySave.positionTestVtxFloat[1] = (float)this->_positionTestVtx32.y / 4096.f;
	outLegacySave.positionTestVtxFloat[2] = (float)this->_positionTestVtx32.z / 4096.f;
	outLegacySave.positionTestVtxFloat[3] = (float)this->_positionTestVtx32.w / 4096.f;
	outLegacySave.boxTestCoord16[0] = this->_boxTestCoord16[0];
	outLegacySave.boxTestCoord16[1] = this->_boxTestCoord16[1];
	outLegacySave.boxTestCoord16[2] = this->_boxTestCoord16[2];
	outLegacySave.boxTestCoord16[3] = this->_boxTestCoord16[3];
	outLegacySave.boxTestCoord16[4] = this->_boxTestCoord16[4];
	outLegacySave.boxTestCoord16[5] = this->_boxTestCoord16[5];
	
	outLegacySave.vtxColor = this->_vtxColor555X;
	
	outLegacySave.regLightColor[0] = this->_regLightColor[0];
	outLegacySave.regLightColor[1] = this->_regLightColor[1];
	outLegacySave.regLightColor[2] = this->_regLightColor[2];
	outLegacySave.regLightColor[3] = this->_regLightColor[3];
	outLegacySave.regLightDirection[0] = this->_regLightDirection[0];
	outLegacySave.regLightDirection[1] = this->_regLightDirection[1];
	outLegacySave.regLightDirection[2] = this->_regLightDirection[2];
	outLegacySave.regLightDirection[3] = this->_regLightDirection[3];
	outLegacySave.regDiffuse  = this->_regDiffuse;
	outLegacySave.regAmbient  = this->_regAmbient;
	outLegacySave.regSpecular = this->_regSpecular;
	outLegacySave.regEmission = this->_regEmission;
	
	memcpy(outLegacySave.shininessTablePending, this->_shininessTablePending, sizeof(outLegacySave.shininessTablePending));
	memcpy(outLegacySave.shininessTableApplied, this->_shininessTableApplied, sizeof(outLegacySave.shininessTableApplied));
	
	outLegacySave.regViewport = this->_regViewport;
	
	outLegacySave.shininessTablePendingIndex = this->_shininessTablePendingIndex;
	
	outLegacySave.generateTriangleStripIndexToggle = (this->_generateTriangleStripIndexToggle) ? 1 : 0;
	outLegacySave.vtxCount = (u32)this->_vtxCount;
	outLegacySave.vtxIndex[0] = (u32)this->_vtxIndex[0];
	outLegacySave.vtxIndex[1] = (u32)this->_vtxIndex[1];
	outLegacySave.vtxIndex[2] = (u32)this->_vtxIndex[2];
	outLegacySave.vtxIndex[3] = (u32)this->_vtxIndex[3];
	outLegacySave.isGeneratingFirstPolyOfStrip = (this->_isGeneratingFirstPolyOfStrip) ? TRUE : FALSE;
}

void NDSGeometryEngine::LoadState_LegacyFormat(const GeometryEngineLegacySave &inLegacySave)
{
	this->_inBegin = (inLegacySave.inBegin) ? true : false;
	this->_texParam = inLegacySave.texParam;
	this->_texPalette = inLegacySave.texPalette;
	
	this->_mtxCurrentMode = (MatrixMode)inLegacySave.mtxCurrentMode;
	MatrixCopy(this->_tempMtxMultiply4x4, inLegacySave.tempMultiplyMatrix);
	MatrixCopy(this->_tempMtxMultiply4x3, inLegacySave.tempMultiplyMatrix);
	MatrixCopy(this->_tempMtxMultiply3x3, inLegacySave.tempMultiplyMatrix);
	MatrixCopy(this->_mtxCurrent[MATRIXMODE_PROJECTION],      inLegacySave.currentMatrix[MATRIXMODE_PROJECTION]);
	MatrixCopy(this->_mtxCurrent[MATRIXMODE_POSITION],        inLegacySave.currentMatrix[MATRIXMODE_POSITION]);
	MatrixCopy(this->_mtxCurrent[MATRIXMODE_POSITION_VECTOR], inLegacySave.currentMatrix[MATRIXMODE_POSITION_VECTOR]);
	MatrixCopy(this->_mtxCurrent[MATRIXMODE_TEXTURE],         inLegacySave.currentMatrix[MATRIXMODE_TEXTURE]);
	this->_mtxLoad4x4PendingIndex = inLegacySave.mtxLoad4x4PendingIndex;
	this->_mtxLoad4x3PendingIndex = inLegacySave.mtxLoad4x3PendingIndex;
	this->_mtxMultiply4x4TempIndex = inLegacySave.mtxMultiply4x4TempIndex;
	this->_mtxMultiply4x3TempIndex = inLegacySave.mtxMultiply4x3TempIndex;
	this->_mtxMultiply3x3TempIndex = inLegacySave.mtxMultiply3x3TempIndex;
	
	this->_vtxCoord16 = inLegacySave.vtxPosition.vec3;
	this->_vtxCoord16CurrentIndex = inLegacySave.vtxPosition16CurrentIndex;
	this->_vtxFormat = (PolygonPrimitiveType)inLegacySave.vtxFormat;
	
	this->_vecTranslate = inLegacySave.vecTranslate;
	this->_vecTranslateCurrentIndex = inLegacySave.vecTranslateCurrentIndex;
	this->_vecScale = inLegacySave.vecScale;
	this->_vecScaleCurrentIndex = inLegacySave.vecScaleCurrentIndex;
	
	this->_texCoord16.t = (s16)((u16)(inLegacySave.texCoordT & 0x0000FFFF));
	this->_texCoord16.s = (s16)((u16)(inLegacySave.texCoordS & 0x0000FFFF));
	this->_texCoordTransformed.t = (s32)inLegacySave.texCoordTransformedT;
	this->_texCoordTransformed.s = (s32)inLegacySave.texCoordTransformedS;
	
	this->_boxTestCoordCurrentIndex = (u8)inLegacySave.boxTestCoordCurrentIndex;
	this->_positionTestCoordCurrentIndex = (u8)inLegacySave.positionTestCoordCurrentIndex;
	this->_positionTestVtx32.x = (s32)((inLegacySave.positionTestVtxFloat[0] * 4096.0f) + 0.000000001f);
	this->_positionTestVtx32.y = (s32)((inLegacySave.positionTestVtxFloat[1] * 4096.0f) + 0.000000001f);
	this->_positionTestVtx32.z = (s32)((inLegacySave.positionTestVtxFloat[2] * 4096.0f) + 0.000000001f);
	this->_positionTestVtx32.w = (s32)((inLegacySave.positionTestVtxFloat[3] * 4096.0f) + 0.000000001f);
	this->_boxTestCoord16[0] = inLegacySave.boxTestCoord16[0];
	this->_boxTestCoord16[1] = inLegacySave.boxTestCoord16[1];
	this->_boxTestCoord16[2] = inLegacySave.boxTestCoord16[2];
	this->_boxTestCoord16[3] = inLegacySave.boxTestCoord16[3];
	this->_boxTestCoord16[4] = inLegacySave.boxTestCoord16[4];
	this->_boxTestCoord16[5] = inLegacySave.boxTestCoord16[5];
	
	this->_vtxColor555X = inLegacySave.vtxColor;
	
	this->_regLightColor[0] = inLegacySave.regLightColor[0];
	this->_regLightColor[1] = inLegacySave.regLightColor[1];
	this->_regLightColor[2] = inLegacySave.regLightColor[2];
	this->_regLightColor[3] = inLegacySave.regLightColor[3];
	this->_regLightDirection[0] = inLegacySave.regLightDirection[0];
	this->_regLightDirection[1] = inLegacySave.regLightDirection[1];
	this->_regLightDirection[2] = inLegacySave.regLightDirection[2];
	this->_regLightDirection[3] = inLegacySave.regLightDirection[3];
	this->_regDiffuse  = inLegacySave.regDiffuse;
	this->_regAmbient  = inLegacySave.regAmbient;
	this->_regSpecular = inLegacySave.regSpecular;
	this->_regEmission = inLegacySave.regEmission;
	
	memcpy(this->_shininessTablePending, inLegacySave.shininessTablePending, sizeof(inLegacySave.shininessTablePending));
	memcpy(this->_shininessTableApplied, inLegacySave.shininessTableApplied, sizeof(inLegacySave.shininessTableApplied));
	
	this->SetViewport(inLegacySave.regViewport);
	
	this->_shininessTablePendingIndex = inLegacySave.shininessTablePendingIndex;
	
	this->_generateTriangleStripIndexToggle = (inLegacySave.generateTriangleStripIndexToggle != 0) ? true : false;
	this->_vtxCount = (size_t)inLegacySave.vtxCount;
	this->_vtxIndex[0] = (u16)inLegacySave.vtxIndex[0];
	this->_vtxIndex[1] = (u16)inLegacySave.vtxIndex[1];
	this->_vtxIndex[2] = (u16)inLegacySave.vtxIndex[2];
	this->_vtxIndex[3] = (u16)inLegacySave.vtxIndex[3];
	this->_isGeneratingFirstPolyOfStrip = (inLegacySave.isGeneratingFirstPolyOfStrip) ? true : false;
}

void NDSGeometryEngine::SaveState_v2(EMUFILE &os)
{
	os.write_32LE((u32)this->_mtxStackIndex[MATRIXMODE_PROJECTION]);
	for (size_t j = 0; j < 16; j++)
	{
		os.write_32LE(this->_mtxStackProjection[0][j]);
	}
	
	os.write_32LE((u32)this->_mtxStackIndex[MATRIXMODE_POSITION]);
	for (size_t i = 0; i < NDSMATRIXSTACK_COUNT(MATRIXMODE_POSITION); i++)
	{
		for (size_t j = 0; j < 16; j++)
		{
			os.write_32LE(this->_mtxStackPosition[i][j]);
		}
	}
	
	os.write_32LE((u32)this->_mtxStackIndex[MATRIXMODE_POSITION_VECTOR]);
	for (size_t i = 0; i < NDSMATRIXSTACK_COUNT(MATRIXMODE_POSITION_VECTOR); i++)
	{
		for (size_t j = 0; j < 16; j++)
		{
			os.write_32LE(_mtxStackPositionVector[i][j]);
		}
	}
	
	os.write_32LE((u32)this->_mtxStackIndex[MATRIXMODE_TEXTURE]);
	for (size_t j = 0; j < 16; j++)
	{
		os.write_32LE(this->_mtxStackTexture[0][j]);
	}
}

void NDSGeometryEngine::LoadState_v2(EMUFILE &is)
{
	u32 loadingIdx;
	
	is.read_32LE(loadingIdx);
	this->_mtxStackIndex[MATRIXMODE_PROJECTION] = (u8)loadingIdx;
	for (size_t j = 0; j < 16; j++)
	{
		is.read_32LE(this->_mtxStackProjection[0][j]);
	}
	
	is.read_32LE(loadingIdx);
	this->_mtxStackIndex[MATRIXMODE_POSITION] = (u8)loadingIdx;
	for (size_t i = 0; i < NDSMATRIXSTACK_COUNT(MATRIXMODE_POSITION); i++)
	{
		for (size_t j = 0; j < 16; j++)
		{
			is.read_32LE(this->_mtxStackPosition[i][j]);
		}
	}
	
	is.read_32LE(loadingIdx);
	this->_mtxStackIndex[MATRIXMODE_POSITION_VECTOR] = (u8)loadingIdx;
	for (size_t i = 0; i < NDSMATRIXSTACK_COUNT(MATRIXMODE_POSITION_VECTOR); i++)
	{
		for (size_t j = 0; j < 16; j++)
		{
			is.read_32LE(this->_mtxStackPositionVector[i][j]);
		}
	}
	
	is.read_32LE(loadingIdx);
	this->_mtxStackIndex[MATRIXMODE_TEXTURE] = (u8)loadingIdx;
	for (size_t j = 0; j < 16; j++)
	{
		is.read_32LE(this->_mtxStackTexture[0][j]);
	}
}

void NDSGeometryEngine::SaveState_v4(EMUFILE &os)
{
	for (size_t i = 0; i < 4; i++)
	{
		os.write_32LE(this->_vecLightDirectionTransformed[i].x);
		os.write_32LE(this->_vecLightDirectionTransformed[i].y);
		os.write_32LE(this->_vecLightDirectionTransformed[i].z);
		os.write_32LE(this->_vecLightDirectionTransformed[i].w);
	}
	
	for (size_t i = 0; i < 4; i++)
	{
		if (this->_doesLightHalfVectorNeedUpdate[i])
		{
			this->UpdateLightDirectionHalfAngleVector(i);
		}
		
		// Historically, these values were saved with the opposite sign.
		os.write_32LE(-this->_vecLightDirectionHalfNegative[i].x);
		os.write_32LE(-this->_vecLightDirectionHalfNegative[i].y);
		os.write_32LE(-this->_vecLightDirectionHalfNegative[i].z);
		os.write_32LE(-this->_vecLightDirectionHalfNegative[i].w);
	}
}

void NDSGeometryEngine::LoadState_v4(EMUFILE &is)
{
	for (size_t i = 0; i < 4; i++)
	{
		is.read_32LE(this->_vecLightDirectionTransformed[i].x);
		is.read_32LE(this->_vecLightDirectionTransformed[i].y);
		is.read_32LE(this->_vecLightDirectionTransformed[i].z);
		is.read_32LE(this->_vecLightDirectionTransformed[i].w);
	}
	
	for (size_t i = 0; i < 4; i++)
	{
		is.read_32LE(this->_vecLightDirectionHalfNegative[i].x);
		is.read_32LE(this->_vecLightDirectionHalfNegative[i].y);
		is.read_32LE(this->_vecLightDirectionHalfNegative[i].z);
		is.read_32LE(this->_vecLightDirectionHalfNegative[i].w);
		
		// Historically, these values were saved with the opposite sign.
		this->_vecLightDirectionHalfNegative[i].x = -this->_vecLightDirectionHalfNegative[i].x;
		this->_vecLightDirectionHalfNegative[i].y = -this->_vecLightDirectionHalfNegative[i].y;
		this->_vecLightDirectionHalfNegative[i].z = -this->_vecLightDirectionHalfNegative[i].z;
		this->_vecLightDirectionHalfNegative[i].w = -this->_vecLightDirectionHalfNegative[i].w;
		this->_doesLightHalfVectorNeedUpdate[i] = false;
	}
}

//===============================================================================
static void gfx3d_glMatrixMode(const u32 param)
{
	_gEngine.SetMatrixMode(param);
	GFX_DELAY(1);
}

static void gfx3d_glPushMatrix()
{
	_gEngine.MatrixPush();
	GFX_DELAY(17);
}

static void gfx3d_glPopMatrix(const u32 param)
{
	_gEngine.MatrixPop(param);
	GFX_DELAY(36);
}

static void gfx3d_glStoreMatrix(const u32 param)
{
	_gEngine.MatrixStore(param);
	GFX_DELAY(17);
}

static void gfx3d_glRestoreMatrix(const u32 param)
{
	_gEngine.MatrixRestore(param);
	GFX_DELAY(36);
}

static void gfx3d_glLoadIdentity()
{
	_gEngine.MatrixLoadIdentityToCurrent();
	GFX_DELAY(19);
}

static void gfx3d_glLoadMatrix4x4(const u32 param)
{
	const bool isMatrixComplete = _gEngine.SetCurrentMatrixLoad4x4(param);
	if (isMatrixComplete)
	{
		_gEngine.MatrixLoad4x4();
		GFX_DELAY(19);
	}
}

static void gfx3d_glLoadMatrix4x3(const u32 param)
{
	const bool isMatrixComplete = _gEngine.SetCurrentMatrixLoad4x3(param);
	if (isMatrixComplete)
	{
		_gEngine.MatrixLoad4x3();
		GFX_DELAY(30);
	}
}

static void gfx3d_glMultMatrix4x4(const u32 param)
{
	const bool isMatrixComplete = _gEngine.SetCurrentMatrixMultiply4x4(param);
	if (isMatrixComplete)
	{
		_gEngine.MatrixMultiply4x4();
		GFX_DELAY(35);
		
		const MatrixMode mtxCurrentMode = _gEngine.GetMatrixMode();
		if (mtxCurrentMode == MATRIXMODE_POSITION_VECTOR)
		{
			GFX_DELAY_M2(30);
		}
	}
}

static void gfx3d_glMultMatrix4x3(const u32 param)
{
	const bool isMatrixComplete = _gEngine.SetCurrentMatrixMultiply4x3(param);
	if (isMatrixComplete)
	{
		_gEngine.MatrixMultiply4x3();
		GFX_DELAY(31);
		
		const MatrixMode mtxCurrentMode = _gEngine.GetMatrixMode();
		if (mtxCurrentMode == MATRIXMODE_POSITION_VECTOR)
		{
			GFX_DELAY_M2(30);
		}
	}
}

static void gfx3d_glMultMatrix3x3(const u32 param)
{
	const bool isMatrixComplete = _gEngine.SetCurrentMatrixMultiply3x3(param);
	if (isMatrixComplete)
	{
		_gEngine.MatrixMultiply3x3();
		GFX_DELAY(28);
		
		const MatrixMode mtxCurrentMode = _gEngine.GetMatrixMode();
		if (mtxCurrentMode == MATRIXMODE_POSITION_VECTOR)
		{
			GFX_DELAY_M2(30);
		}
	}
}

static void gfx3d_glScale(const u32 param)
{
	const bool isVectorComplete = _gEngine.SetCurrentScaleVector(param);
	if (isVectorComplete)
	{
		_gEngine.MatrixScale();
		GFX_DELAY(22);
	}
}

static void gfx3d_glTranslate(const u32 param)
{
	const bool isVectorComplete = _gEngine.SetCurrentTranslateVector(param);
	if (isVectorComplete)
	{
		_gEngine.MatrixTranslate();
		GFX_DELAY(22);
		
		const MatrixMode mtxCurrentMode = _gEngine.GetMatrixMode();
		if (mtxCurrentMode == MATRIXMODE_POSITION_VECTOR)
		{
			GFX_DELAY_M2(30);
		}
	}
}

static void gfx3d_glColor3b(const u32 param)
{
	_gEngine.SetVertexColor(param);
	GFX_DELAY(1);
}

static void gfx3d_glNormal(const u32 param)
{
	_gEngine.SetNormal(param);
	
	GFX_DELAY(9);
	GFX_DELAY_M2(gfx3d.regPolyAttrApplied.Light0);
	GFX_DELAY_M2(gfx3d.regPolyAttrApplied.Light1);
	GFX_DELAY_M2(gfx3d.regPolyAttrApplied.Light2);
	GFX_DELAY_M2(gfx3d.regPolyAttrApplied.Light3);
}

static void gfx3d_glTexCoord(const u32 param)
{
	_gEngine.SetTextureCoordinates2s16(param);
	GFX_DELAY(1);
}

static void gfx3d_glVertex16b(const u32 param)
{
	const bool isVtxComplete = _gEngine.SetCurrentVertexPosition2s16(param);
	if (isVtxComplete)
	{
		_gEngine.AddCurrentVertexToList(gfx3d.gList[gfx3d.pendingListIndex]);
		GFX_DELAY(9);
	}
}

static void gfx3d_glVertex10b(const u32 param)
{
	_gEngine.SetCurrentVertexPosition3s10(param);
	_gEngine.AddCurrentVertexToList(gfx3d.gList[gfx3d.pendingListIndex]);
	GFX_DELAY(8);
}

template <size_t ONE, size_t TWO>
static void gfx3d_glVertex3_cord(const u32 param)
{
	_gEngine.SetCurrentVertexPosition2s16Immediate<ONE, TWO>(param);
	_gEngine.AddCurrentVertexToList(gfx3d.gList[gfx3d.pendingListIndex]);
	GFX_DELAY(8);
}

static void gfx3d_glVertex_rel(const u32 param)
{
	_gEngine.SetCurrentVertexPosition3s10Relative(param);
	_gEngine.AddCurrentVertexToList(gfx3d.gList[gfx3d.pendingListIndex]);
	GFX_DELAY(8);
}

static void gfx3d_glPolygonAttrib(const u32 param)
{
	//if (_inBegin)
	{
		//PROGINFO("Set polyattr in the middle of a begin/end pair.\n  (This won't be activated until the next begin)\n");
		//TODO - we need some some similar checking for teximageparam etc.
	}
	
	gfx3d.regPolyAttrPending.value = param; // Only applied on BEGIN_VTXS
	GFX_DELAY(1);
}

static void gfx3d_glTexImage(const u32 param)
{
	_gEngine.SetTextureParameters(param);
	GFX_DELAY(1);
}

static void gfx3d_glTexPalette(const u32 param)
{
	_gEngine.SetTexturePalette(param);
	GFX_DELAY(1);
}

static void gfx3d_glMaterial0(const u32 param)
{
	_gEngine.SetDiffuseAmbient(param);
	GFX_DELAY(4);
}

static void gfx3d_glMaterial1(const u32 param)
{
	_gEngine.SetSpecularEmission(param);
	GFX_DELAY(4);
}

static void gfx3d_glLightDirection(const u32 param)
{
	_gEngine.SetLightDirection(param);
	GFX_DELAY(6);
}

static void gfx3d_glLightColor(const u32 param)
{
	_gEngine.SetLightColor(param);
	GFX_DELAY(1);
}

static void gfx3d_glShininess(const u32 param)
{
	_gEngine.SetShininess(param);
	GFX_DELAY(32);
}

static void gfx3d_glBegin(const u32 param)
{
	gfx3d.regPolyAttrApplied = gfx3d.regPolyAttrPending;
	
	_gEngine.VertexListBegin(param, gfx3d.regPolyAttrApplied);
	GFX_DELAY(1);
}

static void gfx3d_glEnd()
{
	_gEngine.VertexListEnd();
	GFX_DELAY(1);
}

// swap buffers - skipped

static void gfx3d_glViewport(const u32 param)
{
	_GFX3D_IORegisterMap->VIEWPORT.value = param;
	
	_gEngine.SetViewport(param);
	GFX_DELAY(1);
}

static void gfx3d_glBoxTest(const u32 param)
{
	const bool isBoxTestCoordsCompleted = _gEngine.SetCurrentBoxTestCoords(param);
	if (isBoxTestCoordsCompleted)
	{
		_gEngine.BoxTest();
		GFX_DELAY(103);
	}
}

static void gfx3d_glPosTest(const u32 param)
{
	const bool isPositionTestCoordsCompleted = _gEngine.SetCurrentPositionTestCoords(param);
	if (isPositionTestCoordsCompleted)
	{
		_gEngine.PositionTest();
		GFX_DELAY(9);
	}
}

static void gfx3d_glVecTest(const u32 param)
{
	_gEngine.VectorTest(param);
	GFX_DELAY(5);

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


template <typename T>
void gfx3d_glClearColor(const u8 offset, const T v)
{
	((T *)&gfx3d.pendingState.clearColor)[offset >> (sizeof(T) >> 1)] = v;
}

template void gfx3d_glClearColor< u8>(const u8 offset, const  u8 v);
template void gfx3d_glClearColor<u16>(const u8 offset, const u16 v);
template void gfx3d_glClearColor<u32>(const u8 offset, const u32 v);

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

u32 gfx3d_GetClipMatrix(const u32 index)
{
	return _gEngine.GetClipMatrixAtIndex(index);
}

u32 gfx3d_GetDirectionalMatrix(const u32 index)
{
	return _gEngine.GetDirectionalMatrixAtIndex(index);
}

u32 gfx3d_glGetPosRes(const u32 index)
{
	return _gEngine.GetPositionTestResult(index);
}

void gfx3d_glAlphaFunc(const u32 v)
{
	gfx3d.pendingState.alphaTestRef = (u8)(v & 0x0000001F);
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
	if (gfx3d.isSwapBuffersPending) return;

	//this is a SPEED HACK
	//fifo is currently emulated more accurately than it probably needs to be.
	//without this batch size the emuloop will escape way too often to run fast.
	static const size_t HACK_FIFO_BATCH_SIZE = 64;

	for (size_t i = 0; i < HACK_FIFO_BATCH_SIZE; i++)
	{
		if (GFX_PIPErecv(&cmd, &param))
		{
			//if (gfx3d.isSwapBuffersPending) printf("Executing while swapbuffers is pending: %d:%08X\n",cmd,param);

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
#if 0
	if (gfx3d.isSwapBuffersPending)
	{
		INFO("Error: swapBuffers already use\n");
	}
#endif
	
	gfx3d.pendingState.SWAP_BUFFERS.value = param;
	gfx3d.isSwapBuffersPending = true;

	GFX_DELAY(1);
}

static bool gfx3d_ysort_compare(const u16 idx1, const u16 idx2)
{
	const CPoly &cp1 = gfx3d.clippedPolyUnsortedList[idx1];
	const CPoly &cp2 = gfx3d.clippedPolyUnsortedList[idx2];
	
	const s64 &y1Max = gfx3d.rawPolySortYMax[cp1.index];
	const s64 &y2Max = gfx3d.rawPolySortYMax[cp2.index];
	
	const s64 &y1Min = gfx3d.rawPolySortYMin[cp1.index];
	const s64 &y2Min = gfx3d.rawPolySortYMin[cp2.index];
	
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

static FORCEINLINE s32 iround(const float f)
{
	return (s32)( (f < 0.0f) ? f - 0.5f : f + 0.5f ); //lol
}

template <ClipperMode CLIPPERMODE>
size_t gfx3d_PerformClipping(const GFX3D_GeometryList &gList, CPoly *outCPolyUnsortedList)
{
	size_t clipCount = 0;
	PolygonType cpType = POLYGON_TYPE_UNDEFINED;
	
	const s64 wScalar = (s64)CurrentRenderer->GetFramebufferWidth()  / (s64)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const s64 hScalar = (s64)CurrentRenderer->GetFramebufferHeight() / (s64)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	for (size_t polyIndex = 0; polyIndex < gList.rawPolyCount; polyIndex++)
	{
		const POLY &rawPoly = gList.rawPolyList[polyIndex];
		const GFX3D_Viewport theViewport = rawPoly.viewport;
		
		const NDSVertex *rawPolyVtx[4] = {
			&gList.rawVtxList[rawPoly.vertIndexes[0]],
			&gList.rawVtxList[rawPoly.vertIndexes[1]],
			&gList.rawVtxList[rawPoly.vertIndexes[2]],
			(rawPoly.type == POLYGON_TYPE_QUAD) ? &gList.rawVtxList[rawPoly.vertIndexes[3]] : NULL
		};
		
		CPoly &cPoly = outCPolyUnsortedList[clipCount];
		
		cpType = GFX3D_GenerateClippedPoly<CLIPPERMODE>(polyIndex, rawPoly.type, rawPolyVtx, cPoly);
		if (cpType == POLYGON_TYPE_UNDEFINED)
		{
			continue;
		}
		
		for (size_t j = 0; j < (size_t)cPoly.type; j++)
		{
			NDSVertex &vtx = cPoly.vtx[j];
			Vector4s64 vtx64 = {
				(s64)vtx.position.x,
				(s64)vtx.position.y,
				(s64)vtx.position.z,
				(s64)vtx.position.w,
			};
			
			//homogeneous divide
			if (vtx64.w != 0)
			{
				vtx64.x = ((vtx64.x + vtx64.w) * ((s64)theViewport.width  << 16)) / (2 * vtx64.w);
				vtx64.y = ((vtx64.y + vtx64.w) * ((s64)theViewport.height << 16)) / (2 * vtx64.w);
				vtx64.z = ((vtx64.z + vtx64.w) * (                    1LL << 31)) / (2 * vtx64.w);
				
				// Convert Z from 20.12 to 20.43 since we need to preserve as much precision
				// as possible for Z-depth calculations. Several games need the precision in
				// order to prevent missing polygons, maintain correct coloring, draw 2D-on-3D
				// animations, and other types of 3D scenarios.
			}
			else
			{
				vtx64.x = (vtx64.x * ((s64)theViewport.width  << 16));
				vtx64.y = (vtx64.y * ((s64)theViewport.height << 16));
				vtx64.z = (vtx64.z * (                    1LL << 31)); // See comments above for why we need to convert Z to 22.42.
			}
			
			// Finish viewport transformation.
			vtx64.x += ((s64)theViewport.x << 16);
			vtx64.x *= wScalar;
			
			vtx64.y += ((s64)theViewport.y << 16);
			vtx64.y = (192LL << 16) - vtx64.y;
			vtx64.y *= hScalar;
			
			// We need to fit the 64-bit Z into a 32-bit integer, so we will need to drop some bits.
			// - Divide by w = 20.43 --> 20.31
			// - Divide by 2 = 20.31 --> 20.30
			// - Keep the sign bit = 20.30 --> 20.31
			vtx64.z = std::max<s64>(0x0000000000000000LL, std::min<s64>(0x000000007FFFFFFFLL, vtx64.z));
			
			// At the very least, we need to save the transformed position so that
			// we can use it to calculate the polygon facing later.
			vtx.position.x = (s32)vtx64.x; // 16.16
			vtx.position.y = (s32)vtx64.y; // 16.16
			vtx.position.z = (s32)vtx64.z; // 0.31
			vtx.position.w = (s32)vtx64.w; // 20.12
		}
		
		// Determine the polygon facing.
		
		//an older approach
		//(not good enough for quads and other shapes)
		//float ab[2], ac[2]; Vector2Copy(ab, verts[1].coord); Vector2Copy(ac, verts[2].coord); Vector2Subtract(ab, verts[0].coord);
		//Vector2Subtract(ac, verts[0].coord); float cross = Vector2Cross(ab, ac); polyAttr.backfacing = (cross>0);
		
		//a better approach
		// we have to support somewhat non-convex polygons (see NSMB world map 1st screen).
		// this version should handle those cases better.
		
		const NDSVertex *vtx = cPoly.vtx;
		const size_t n = cPoly.type - 1;
		s64 facing = ((s64)vtx[0].position.y + (s64)vtx[n].position.y) * ((s64)vtx[0].position.x - (s64)vtx[n].position.x) +
		             ((s64)vtx[1].position.y + (s64)vtx[0].position.y) * ((s64)vtx[1].position.x - (s64)vtx[0].position.x) +
		             ((s64)vtx[2].position.y + (s64)vtx[1].position.y) * ((s64)vtx[2].position.x - (s64)vtx[1].position.x);
		
		for (size_t j = 2; j < n; j++)
		{
			facing += ((s64)vtx[j+1].position.y + (s64)vtx[j].position.y) * ((s64)vtx[j+1].position.x - (s64)vtx[j].position.x);
		}
		
		cPoly.isPolyBackFacing = (facing < 0);
		
		// Perform face culling.
		static const bool visibleFunction[2][4] = {
			//always false, backfacing, !backfacing, always true
			{ false, false, true, true },
			{ false, true, false, true }
		};
		
		const u8 cullingMode = rawPoly.attribute.SurfaceCullingMode;
		const bool isPolyVisible = visibleFunction[(cPoly.isPolyBackFacing) ? 1 : 0][cullingMode];
		if (!isPolyVisible)
		{
			// If the polygon is to be culled, then don't count it.
			continue;
		}
		
		// Incrementing the count will keep the polygon in the list.
		clipCount++;
	}
	
	return clipCount;
}

void GFX3D_GenerateRenderLists(const ClipperMode clippingMode, const GFX3D_State &inState, GFX3D_GeometryList &outGList)
{
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
	max_polys = std::max((u32)outGList.rawPolyCount, max_polys);
	max_verts = std::max((u32)outGList.rawVertCount, max_verts);
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
	const NDSVertex *__restrict appliedVertList = outGList.rawVtxList;
	
	for (size_t i = 0; i < outGList.clippedPolyCount; i++)
	{
		const u16 rawPolyIndex = gfx3d.clippedPolyUnsortedList[i].index;
		const POLY &poly = outGList.rawPolyList[rawPolyIndex];
		
		s64 y = (s64)appliedVertList[poly.vertIndexes[0]].position.y;
		s64 w = (s64)appliedVertList[poly.vertIndexes[0]].position.w;
		
		// TODO: Possible divide by zero with the w-coordinate.
		// Is the vertex being read correctly? Is 0 a valid value for w?
		// If both of these questions answer to yes, then how does the NDS handle a NaN?
		// For now, simply ignore w if it is zero.
		if (w != 0)
		{
			y = ((y * 4096LL) + (w * 4096LL)) / (2LL * w);
		}
		
		y = 4096LL - y;
		
		gfx3d.rawPolySortYMin[rawPolyIndex] = y;
		gfx3d.rawPolySortYMax[rawPolyIndex] = y;
		
		for (size_t j = 1; j < (size_t)poly.type; j++)
		{
			y = (s64)appliedVertList[poly.vertIndexes[j]].position.y;
			w = (s64)appliedVertList[poly.vertIndexes[j]].position.w;
			
			if (w != 0)
			{
				y = ((y * 4096LL) + (w * 4096LL)) / (2LL * w);
			}
			
			y = 4096LL - y;
			
			gfx3d.rawPolySortYMin[rawPolyIndex] = std::min<s64>(gfx3d.rawPolySortYMin[rawPolyIndex], y);
			gfx3d.rawPolySortYMax[rawPolyIndex] = std::max<s64>(gfx3d.rawPolySortYMax[rawPolyIndex], y);
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
		
		memcpy(viewer3D.gList.rawVtxList, appliedGList.rawVtxList, appliedGList.rawVertCount * sizeof(NDSVertex));
		memcpy(viewer3D.gList.rawPolyList, appliedGList.rawPolyList, appliedGList.rawPolyCount * sizeof(POLY));
		memcpy(viewer3D.gList.clippedPolyList, appliedGList.clippedPolyList, appliedGList.clippedPolyCount * sizeof(CPoly));
		
		driver->view3d->NewFrame();
	}
	
	gfx3d.render3DFrameCount++;
	gfx3d.isDrawPending = true;
}

void gfx3d_VBlankSignal()
{
	if (gfx3d.isSwapBuffersPending)
	{
		gfx3d_doFlush();
		GFX_DELAY(392);
		gfx3d.isSwapBuffersPending = false;
		
		//let's consider this the beginning of the next 3d frame.
		//in case the game isn't constantly restoring the projection matrix, we want to ping lua
		_gEngine.UpdateMatrixProjectionLua();
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
	
	if (!gfx3d.isDrawPending && !forceDraw)
		return;

	if (skipFrame) return;

	gfx3d.isDrawPending = false;
	
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
template <MatrixMode MODE>
void gfx3d_glGetMatrix(const int index, float (&dst)[16])
{
	if (index == -1)
	{
		_gEngine.MatrixCopyFromCurrent(MODE, dst);
	}
	else
	{
		// Theoretically, we COULD use the index parameter here to choose which
		// specific matrix to retrieve from the matrix stack, but historically,
		// this call always returned the matrix at stack location 0.
		_gEngine.MatrixCopyFromStack(MODE, 0, dst);
	}
}

void gfx3d_glGetLightDirection(const size_t index, u32 &dst)
{
	dst = _gEngine.GetLightDirectionRegisterAtIndex(index);
}

void gfx3d_glGetLightColor(const size_t index, u32 &dst)
{
	dst = _gEngine.GetLightColorRegisterAtIndex(index);
}

//http://www.opengl.org/documentation/specs/version1.1/glspec1.1/node17.html
//talks about the state required to process verts in quadlists etc. helpful ideas.
//consider building a little state structure that looks exactly like this describes

SFORMAT SF_GFX3D[] = {
	{ "GCTL", 4, 1, &gfx3d.pendingState.DISP3DCNT},
	{ "GPAT", 4, 1, &gfx3d.regPolyAttrApplied},
	{ "GPAP", 4, 1, &gfx3d.regPolyAttrPending},
	{ "GINB", 4, 1, &gfx3d.gEngineLegacySave.inBegin},
	{ "GTFM", 4, 1, &gfx3d.gEngineLegacySave.texParam},
	{ "GTPA", 4, 1, &gfx3d.gEngineLegacySave.texPalette},
	{ "GMOD", 4, 1, &gfx3d.gEngineLegacySave.mtxCurrentMode},
	{ "GMTM", 4,16, &gfx3d.gEngineLegacySave.tempMultiplyMatrix},
	{ "GMCU", 4,64, &gfx3d.gEngineLegacySave.currentMatrix},
	{ "ML4I", 1, 1, &gfx3d.gEngineLegacySave.mtxLoad4x4PendingIndex},
	{ "ML3I", 1, 1, &gfx3d.gEngineLegacySave.mtxLoad4x3PendingIndex},
	{ "MM4I", 1, 1, &gfx3d.gEngineLegacySave.mtxMultiply4x4TempIndex},
	{ "MM3I", 1, 1, &gfx3d.gEngineLegacySave.mtxMultiply4x3TempIndex},
	{ "MMxI", 1, 1, &gfx3d.gEngineLegacySave.mtxMultiply3x3TempIndex},
	{ "GSCO", 2, 4, &gfx3d.gEngineLegacySave.vtxPosition},
	{ "GCOI", 1, 1, &gfx3d.gEngineLegacySave.vtxPosition16CurrentIndex},
	{ "GVFM", 4, 1, &gfx3d.gEngineLegacySave.vtxFormat},
	{ "GTRN", 4, 4, &gfx3d.gEngineLegacySave.vecTranslate},
	{ "GTRI", 1, 1, &gfx3d.gEngineLegacySave.vecTranslateCurrentIndex},
	{ "GSCA", 4, 4, &gfx3d.gEngineLegacySave.vecScale},
	{ "GSCI", 1, 1, &gfx3d.gEngineLegacySave.vecScaleCurrentIndex},
	{ "G_T_", 4, 1, &gfx3d.gEngineLegacySave.texCoordT},
	{ "G_S_", 4, 1, &gfx3d.gEngineLegacySave.texCoordS},
	{ "GL_T", 4, 1, &gfx3d.gEngineLegacySave.texCoordTransformedT},
	{ "GL_S", 4, 1, &gfx3d.gEngineLegacySave.texCoordTransformedS},
	{ "GLCM", 4, 1, &gfx3d.legacySave.clCommand},
	{ "GLIN", 4, 1, &gfx3d.legacySave.clIndex},
	{ "GLI2", 4, 1, &gfx3d.legacySave.clIndex2},
	{ "GLSB", 4, 1, &gfx3d.legacySave.isSwapBuffersPending},
	{ "GLBT", 4, 1, &gfx3d.gEngineLegacySave.boxTestCoordCurrentIndex},
	{ "GLPT", 4, 1, &gfx3d.gEngineLegacySave.positionTestCoordCurrentIndex},
	{ "GLPC", 4, 4,  gfx3d.gEngineLegacySave.positionTestVtxFloat},
	{ "GBTC", 2, 6,  gfx3d.gEngineLegacySave.boxTestCoord16},
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
	{ "GCOL", 1, 4, &gfx3d.gEngineLegacySave.vtxColor},
	{ "GLCO", 4, 4,  gfx3d.gEngineLegacySave.regLightColor},
	{ "GLDI", 4, 4,  gfx3d.gEngineLegacySave.regLightDirection},
	{ "GMDI", 2, 1, &gfx3d.gEngineLegacySave.regDiffuse},
	{ "GMAM", 2, 1, &gfx3d.gEngineLegacySave.regAmbient},
	{ "GMSP", 2, 1, &gfx3d.gEngineLegacySave.regSpecular},
	{ "GMEM", 2, 1, &gfx3d.gEngineLegacySave.regEmission},
	{ "GDRP", 4, 1, &gfx3d.legacySave.isDrawPending},
	{ "GSET", 4, 1, &gfx3d.legacySave.statePending.enableTexturing},
	{ "GSEA", 4, 1, &gfx3d.legacySave.statePending.enableAlphaTest},
	{ "GSEB", 4, 1, &gfx3d.legacySave.statePending.enableAlphaBlending},
	{ "GSEX", 4, 1, &gfx3d.legacySave.statePending.enableAntialiasing},
	{ "GSEE", 4, 1, &gfx3d.legacySave.statePending.enableEdgeMarking},
	{ "GSEC", 4, 1, &gfx3d.legacySave.statePending.enableClearImage},
	{ "GSEF", 4, 1, &gfx3d.legacySave.statePending.enableFog},
	{ "GSEO", 4, 1, &gfx3d.legacySave.statePending.enableFogAlphaOnly},
	{ "GFSH", 4, 1, &gfx3d.legacySave.statePending.fogShift},
	{ "GSSH", 4, 1, &gfx3d.legacySave.statePending.toonShadingMode},
	{ "GSWB", 4, 1, &gfx3d.legacySave.statePending.enableWDepth},
	{ "GSSM", 4, 1, &gfx3d.legacySave.statePending.polygonTransparentSortMode},
	{ "GSAR", 1, 1, &gfx3d.legacySave.statePending.alphaTestRef},
	{ "GSCC", 4, 1, &gfx3d.legacySave.statePending.clearColor},
	{ "GSCD", 4, 1, &gfx3d.legacySave.statePending.clearDepth},
	{ "GSFC", 4, 4,  gfx3d.legacySave.statePending.fogColor},
	{ "GSFO", 4, 1, &gfx3d.legacySave.statePending.fogOffset},
	{ "GST4", 2, 32, gfx3d.legacySave.statePending.toonTable16},
	{ "GSSU", 1, 128, gfx3d.gEngineLegacySave.shininessTablePending},
	{ "GSAF", 4, 1, &gfx3d.legacySave.statePending.activeFlushCommand},
	{ "GSPF", 4, 1, &gfx3d.legacySave.statePending.pendingFlushCommand},

	{ "gSET", 4, 1, &gfx3d.legacySave.stateApplied.enableTexturing},
	{ "gSEA", 4, 1, &gfx3d.legacySave.stateApplied.enableAlphaTest},
	{ "gSEB", 4, 1, &gfx3d.legacySave.stateApplied.enableAlphaBlending},
	{ "gSEX", 4, 1, &gfx3d.legacySave.stateApplied.enableAntialiasing},
	{ "gSEE", 4, 1, &gfx3d.legacySave.stateApplied.enableEdgeMarking},
	{ "gSEC", 4, 1, &gfx3d.legacySave.stateApplied.enableClearImage},
	{ "gSEF", 4, 1, &gfx3d.legacySave.stateApplied.enableFog},
	{ "gSEO", 4, 1, &gfx3d.legacySave.stateApplied.enableFogAlphaOnly},
	{ "gFSH", 4, 1, &gfx3d.legacySave.stateApplied.fogShift},
	{ "gSSH", 4, 1, &gfx3d.legacySave.stateApplied.toonShadingMode},
	{ "gSWB", 4, 1, &gfx3d.legacySave.stateApplied.enableWDepth},
	{ "gSSM", 4, 1, &gfx3d.legacySave.stateApplied.polygonTransparentSortMode},
	{ "gSAR", 1, 1, &gfx3d.legacySave.stateApplied.alphaTestRef},
	{ "gSCC", 4, 1, &gfx3d.legacySave.stateApplied.clearColor},
	{ "gSCD", 4, 1, &gfx3d.legacySave.stateApplied.clearDepth},
	{ "gSFC", 4, 4,  gfx3d.legacySave.stateApplied.fogColor},
	{ "gSFO", 4, 1, &gfx3d.legacySave.stateApplied.fogOffset},
	{ "gST4", 2, 32, gfx3d.legacySave.stateApplied.toonTable16},
	{ "gSSU", 1, 128, gfx3d.gEngineLegacySave.shininessTableApplied},
	{ "gSAF", 4, 1, &gfx3d.legacySave.stateApplied.activeFlushCommand},
	{ "gSPF", 4, 1, &gfx3d.legacySave.stateApplied.pendingFlushCommand},

	{ "GSVP", 4, 1, &gfx3d.gEngineLegacySave.regViewport},
	{ "GSSI", 1, 1, &gfx3d.gEngineLegacySave.shininessTablePendingIndex},
	//------------------------
	{ "GTST", 1, 1, &gfx3d.gEngineLegacySave.generateTriangleStripIndexToggle},
	{ "GTVC", 4, 1, &gfx3d.gEngineLegacySave.vtxCount},
	{ "GTVM", 4, 4,  gfx3d.gEngineLegacySave.vtxIndex},
	{ "GTVF", 4, 1, &gfx3d.gEngineLegacySave.isGeneratingFirstPolyOfStrip},
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
		const Color4u8 *__restrict src = (Color4u8 *)CurrentRenderer->GetFramebuffer();
		Color4u8 *__restrict dst = gfx3d.framebufferNativeSave;
		
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
	_gEngine.SaveState_LegacyFormat(gfx3d.gEngineLegacySave);
	
	gfx3d.legacySave.isSwapBuffersPending = (gfx3d.isSwapBuffersPending) ? TRUE : FALSE;
	gfx3d.legacySave.isDrawPending = (gfx3d.isDrawPending) ? TRUE : FALSE;
	
	gfx3d.legacySave.statePending.enableTexturing     = (gfx3d.pendingState.DISP3DCNT.EnableTexMapping)    ? TRUE : FALSE;
	gfx3d.legacySave.statePending.enableAlphaTest     = (gfx3d.pendingState.DISP3DCNT.EnableAlphaTest)     ? TRUE : FALSE;
	gfx3d.legacySave.statePending.enableAlphaBlending = (gfx3d.pendingState.DISP3DCNT.EnableAlphaBlending) ? TRUE : FALSE;
	gfx3d.legacySave.statePending.enableAntialiasing  = (gfx3d.pendingState.DISP3DCNT.EnableAntialiasing)  ? TRUE : FALSE;
	gfx3d.legacySave.statePending.enableEdgeMarking   = (gfx3d.pendingState.DISP3DCNT.EnableEdgeMarking)   ? TRUE : FALSE;
	gfx3d.legacySave.statePending.enableClearImage    = (gfx3d.pendingState.DISP3DCNT.RearPlaneMode)       ? TRUE : FALSE;
	gfx3d.legacySave.statePending.enableFog           = (gfx3d.pendingState.DISP3DCNT.EnableFog)           ? TRUE : FALSE;
	gfx3d.legacySave.statePending.enableFogAlphaOnly  = (gfx3d.pendingState.DISP3DCNT.FogOnlyAlpha)        ? TRUE : FALSE;
	gfx3d.legacySave.statePending.fogShift            = gfx3d.pendingState.fogShift;
	gfx3d.legacySave.statePending.toonShadingMode     = gfx3d.pendingState.DISP3DCNT.PolygonShading;
	gfx3d.legacySave.statePending.enableWDepth        = (gfx3d.pendingState.SWAP_BUFFERS.DepthMode)        ? TRUE : FALSE;
	gfx3d.legacySave.statePending.polygonTransparentSortMode = (gfx3d.pendingState.SWAP_BUFFERS.YSortMode) ? TRUE : FALSE;
	gfx3d.legacySave.statePending.alphaTestRef        = gfx3d.pendingState.alphaTestRef;
	gfx3d.legacySave.statePending.clearColor          = gfx3d.pendingState.clearColor;
	gfx3d.legacySave.statePending.clearDepth          = gfx3d.pendingState.clearDepth;
	gfx3d.legacySave.statePending.fogColor[0]         = gfx3d.pendingState.fogColor;
	gfx3d.legacySave.statePending.fogColor[1]         = gfx3d.pendingState.fogColor;
	gfx3d.legacySave.statePending.fogColor[2]         = gfx3d.pendingState.fogColor;
	gfx3d.legacySave.statePending.fogColor[3]         = gfx3d.pendingState.fogColor;
	gfx3d.legacySave.statePending.fogOffset           = gfx3d.pendingState.fogOffset;
	gfx3d.legacySave.statePending.activeFlushCommand  = gfx3d.appliedState.SWAP_BUFFERS.value;
	gfx3d.legacySave.statePending.pendingFlushCommand = gfx3d.pendingState.SWAP_BUFFERS.value;
	memcpy(gfx3d.legacySave.statePending.toonTable16, gfx3d.pendingState.toonTable16, sizeof(gfx3d.pendingState.toonTable16));
	
	gfx3d.legacySave.stateApplied.enableTexturing     = (gfx3d.appliedState.DISP3DCNT.EnableTexMapping)    ? TRUE : FALSE;
	gfx3d.legacySave.stateApplied.enableAlphaTest     = (gfx3d.appliedState.DISP3DCNT.EnableAlphaTest)     ? TRUE : FALSE;
	gfx3d.legacySave.stateApplied.enableAlphaBlending = (gfx3d.appliedState.DISP3DCNT.EnableAlphaBlending) ? TRUE : FALSE;
	gfx3d.legacySave.stateApplied.enableAntialiasing  = (gfx3d.appliedState.DISP3DCNT.EnableAntialiasing)  ? TRUE : FALSE;
	gfx3d.legacySave.stateApplied.enableEdgeMarking   = (gfx3d.appliedState.DISP3DCNT.EnableEdgeMarking)   ? TRUE : FALSE;
	gfx3d.legacySave.stateApplied.enableClearImage    = (gfx3d.appliedState.DISP3DCNT.RearPlaneMode)       ? TRUE : FALSE;
	gfx3d.legacySave.stateApplied.enableFog           = (gfx3d.appliedState.DISP3DCNT.EnableFog)           ? TRUE : FALSE;
	gfx3d.legacySave.stateApplied.enableFogAlphaOnly  = (gfx3d.appliedState.DISP3DCNT.FogOnlyAlpha)        ? TRUE : FALSE;
	gfx3d.legacySave.stateApplied.fogShift            = gfx3d.appliedState.fogShift;
	gfx3d.legacySave.stateApplied.toonShadingMode     = gfx3d.appliedState.DISP3DCNT.PolygonShading;
	gfx3d.legacySave.stateApplied.enableWDepth        = (gfx3d.appliedState.SWAP_BUFFERS.DepthMode)        ? TRUE : FALSE;
	gfx3d.legacySave.stateApplied.polygonTransparentSortMode = (gfx3d.appliedState.SWAP_BUFFERS.YSortMode) ? TRUE : FALSE;
	gfx3d.legacySave.stateApplied.alphaTestRef        = gfx3d.appliedState.alphaTestRef;
	gfx3d.legacySave.stateApplied.clearColor          = gfx3d.appliedState.clearColor;
	gfx3d.legacySave.stateApplied.clearDepth          = gfx3d.appliedState.clearDepth;
	gfx3d.legacySave.stateApplied.fogColor[0]         = gfx3d.appliedState.fogColor;
	gfx3d.legacySave.stateApplied.fogColor[1]         = gfx3d.appliedState.fogColor;
	gfx3d.legacySave.stateApplied.fogColor[2]         = gfx3d.appliedState.fogColor;
	gfx3d.legacySave.stateApplied.fogColor[3]         = gfx3d.appliedState.fogColor;
	gfx3d.legacySave.stateApplied.fogOffset           = gfx3d.appliedState.fogOffset;
	gfx3d.legacySave.stateApplied.activeFlushCommand  = gfx3d.appliedState.SWAP_BUFFERS.value;
	gfx3d.legacySave.stateApplied.pendingFlushCommand = gfx3d.pendingState.SWAP_BUFFERS.value;
	memcpy(gfx3d.legacySave.stateApplied.toonTable16, gfx3d.appliedState.toonTable16, sizeof(gfx3d.appliedState.toonTable16));
}

void gfx3d_savestate(EMUFILE &os)
{
	//version
	os.write_32LE(4);

	//dump the render lists
	os.write_32LE((u32)gfx3d.gList[gfx3d.pendingListIndex].rawVertCount);
	for (size_t i = 0; i < gfx3d.gList[gfx3d.pendingListIndex].rawVertCount; i++)
	{
		GFX3D_SaveStateVertex(gfx3d.gList[gfx3d.pendingListIndex].rawVtxList[i], os);
	}
	
	os.write_32LE((u32)gfx3d.gList[gfx3d.pendingListIndex].rawPolyCount);
	for (size_t i = 0; i < gfx3d.gList[gfx3d.pendingListIndex].rawPolyCount; i++)
	{
		GFX3D_SaveStatePOLY(gfx3d.gList[gfx3d.pendingListIndex].rawPolyList[i], os);
		os.write_32LE(gfx3d.legacySave.rawPolyViewport[i].value);
		os.write_floatLE( (float)((double)gfx3d.rawPolySortYMin[i] / 4096.0) );
		os.write_floatLE( (float)((double)gfx3d.rawPolySortYMax[i] / 4096.0) );
	}
	
	_gEngine.SaveState_v2(os);
	gxf_hardware.savestate(os);
	_gEngine.SaveState_v4(os);
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

	gfx3d_parseCurrentDISP3DCNT();
	
	if (version >= 1)
	{
		u32 vertListCount32 = 0;
		u32 polyListCount32 = 0;
		float loadingSortYMin = 0.0f;
		float loadingSortYMax = 0.0f;
		
		is.read_32LE(vertListCount32);
		gfx3d.gList[gfx3d.pendingListIndex].rawVertCount = vertListCount32;
		gfx3d.gList[gfx3d.appliedListIndex].rawVertCount = vertListCount32;
		for (size_t i = 0; i < gfx3d.gList[gfx3d.appliedListIndex].rawVertCount; i++)
		{
			GFX3D_LoadStateVertex(gfx3d.gList[gfx3d.pendingListIndex].rawVtxList[i], is);
			gfx3d.gList[gfx3d.appliedListIndex].rawVtxList[i] = gfx3d.gList[gfx3d.pendingListIndex].rawVtxList[i];
		}
		
		is.read_32LE(polyListCount32);
		gfx3d.gList[gfx3d.pendingListIndex].rawPolyCount = polyListCount32;
		gfx3d.gList[gfx3d.appliedListIndex].rawPolyCount = polyListCount32;
		for (size_t i = 0; i < gfx3d.gList[gfx3d.appliedListIndex].rawPolyCount; i++)
		{
			POLY &p = gfx3d.gList[gfx3d.pendingListIndex].rawPolyList[i];
			
			GFX3D_LoadStatePOLY(p, is);
			is.read_32LE(gfx3d.legacySave.rawPolyViewport[i].value);
			is.read_floatLE(loadingSortYMin);
			is.read_floatLE(loadingSortYMax);
			
			p.viewport = GFX3D_ViewportParse(gfx3d.legacySave.rawPolyViewport[i]);
			gfx3d.rawPolySortYMin[i] = (s64)( (loadingSortYMin * 4096.0f) + 0.5f );
			gfx3d.rawPolySortYMax[i] = (s64)( (loadingSortYMax * 4096.0f) + 0.5f );
			
			gfx3d.gList[gfx3d.appliedListIndex].rawPolyList[i] = p;
		}
	}

	if (version >= 2)
	{
		_gEngine.LoadState_v2(is);
	}

	if (version >= 3)
	{
		gxf_hardware.loadstate(is);
	}

	if (version >= 4)
	{
		_gEngine.LoadState_v4(is);
	}

	return true;
}

void gfx3d_FinishLoadStateBufferRead()
{
	const Render3DDeviceInfo &deviceInfo = CurrentRenderer->GetDeviceInfo();
	
	switch (deviceInfo.renderID)
	{
		case RENDERID_NULL:
			memset(CurrentRenderer->GetFramebuffer(), 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(Color4u8));
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
				
				const Color4u8 *__restrict src = gfx3d.framebufferNativeSave;
				Color4u8 *__restrict dst = (Color4u8 *)CurrentRenderer->GetFramebuffer();
				
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
	
	_gEngine.LoadState_LegacyFormat(gfx3d.gEngineLegacySave);
	
	gfx3d.isSwapBuffersPending = (gfx3d.legacySave.isSwapBuffersPending) ? true : false;
	gfx3d.isDrawPending = (gfx3d.legacySave.isDrawPending) ? true : false;
	
	gfx3d.pendingState.DISP3DCNT = GPUREG.DISP3DCNT;
	gfx3d_parseCurrentDISP3DCNT();
	
	gfx3d.pendingState.SWAP_BUFFERS = GFX3DREG.SWAP_BUFFERS;
	gfx3d.pendingState.clearColor = gfx3d.legacySave.statePending.clearColor;
	gfx3d.pendingState.clearDepth = gfx3d.legacySave.statePending.clearDepth;
	gfx3d.pendingState.fogColor = gfx3d.legacySave.statePending.fogColor[0];
	gfx3d.pendingState.fogOffset = gfx3d.legacySave.statePending.fogOffset;
	gfx3d.pendingState.alphaTestRef = gfx3d.legacySave.statePending.alphaTestRef;
	memcpy(gfx3d.pendingState.toonTable16, gfx3d.legacySave.statePending.toonTable16, sizeof(gfx3d.pendingState.toonTable16));
	memcpy(gfx3d.pendingState.edgeMarkColorTable, GFX3DREG.EDGE_COLOR, sizeof(GFX3DREG.EDGE_COLOR));
	memcpy(gfx3d.pendingState.fogDensityTable, GFX3DREG.FOG_TABLE, sizeof(GFX3DREG.FOG_TABLE));
	
	gfx3d.appliedState.DISP3DCNT.value               = 0;
	gfx3d.appliedState.DISP3DCNT.EnableTexMapping    = (gfx3d.legacySave.stateApplied.enableTexturing)     ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.EnableAlphaTest     = (gfx3d.legacySave.stateApplied.enableAlphaTest)     ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.EnableAlphaBlending = (gfx3d.legacySave.stateApplied.enableAlphaBlending) ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.EnableAntialiasing  = (gfx3d.legacySave.stateApplied.enableAntialiasing)  ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.EnableEdgeMarking   = (gfx3d.legacySave.stateApplied.enableEdgeMarking)   ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.RearPlaneMode       = (gfx3d.legacySave.stateApplied.enableClearImage)    ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.EnableFog           = (gfx3d.legacySave.stateApplied.enableFog)           ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.FogOnlyAlpha        = (gfx3d.legacySave.stateApplied.enableFogAlphaOnly)  ? 1 : 0;
	gfx3d.appliedState.DISP3DCNT.PolygonShading      = (gfx3d.legacySave.stateApplied.toonShadingMode)     ? 1 : 0;
	gfx3d.appliedState.SWAP_BUFFERS.value            = 0;
	gfx3d.appliedState.SWAP_BUFFERS.DepthMode        = (gfx3d.legacySave.stateApplied.enableWDepth)        ? 1 : 0;
	gfx3d.appliedState.SWAP_BUFFERS.YSortMode        = (gfx3d.legacySave.stateApplied.polygonTransparentSortMode) ? 1 : 0;
	gfx3d.appliedState.fogShift                      = gfx3d.legacySave.stateApplied.fogShift;
	gfx3d.appliedState.clearColor                    = gfx3d.legacySave.stateApplied.clearColor;
	gfx3d.appliedState.clearDepth                    = gfx3d.legacySave.stateApplied.clearDepth;
	gfx3d.appliedState.fogColor                      = gfx3d.legacySave.stateApplied.fogColor[0];
	gfx3d.appliedState.fogOffset                     = gfx3d.legacySave.stateApplied.fogOffset;
	gfx3d.appliedState.alphaTestRef                  = gfx3d.legacySave.stateApplied.alphaTestRef;
	gfx3d.appliedState.SWAP_BUFFERS.value            = gfx3d.legacySave.stateApplied.activeFlushCommand;
	memcpy(gfx3d.appliedState.toonTable16, gfx3d.legacySave.stateApplied.toonTable16, sizeof(gfx3d.appliedState.toonTable16));
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

u8 GFX3D_GetMatrixStackIndex(const MatrixMode whichMatrix)
{
	return _gEngine.GetMatrixStackIndex(whichMatrix);
}

void GFX3D_ResetMatrixStackPointer()
{
	_gEngine.ResetMatrixStackPointer();
}

bool GFX3D_IsSwapBuffersPending()
{
	return gfx3d.isSwapBuffersPending;
}

void GFX3D_HandleGeometryPowerOff()
{
	//kill the geometry data when the power goes off
	//so, so bad. we need to model this with hardware-like operations instead of c++ code
	
	// TODO: Test which geometry data should be cleared on power off.
	// The code below doesn't make any sense. You would think that only the data that
	// is derived from geometry commands (either via GXFIFO or if writing to registers
	// 0x04000440 - 0x040006A4) is what should be cleared. And that outside of geometry
	// command data, other data (even if related to the 3D engine) shouldn't be touched.
	// This will need further testing, but for now, we'll leave things as they are.
	//    - 2023/01/22, rogerman
	gfx3d.pendingState.DISP3DCNT.value = 0;
	gfx3d.pendingState.DISP3DCNT.EnableTexMapping = 1;
	gfx3d.pendingState.DISP3DCNT.PolygonShading = PolygonShadingMode_Toon;
	gfx3d.pendingState.DISP3DCNT.EnableAlphaTest = 1;
	gfx3d.pendingState.DISP3DCNT.EnableAlphaBlending = 1;
	gfx3d.pendingState.SWAP_BUFFERS.value = 0;
	gfx3d.pendingState.alphaTestRef = 0;
	gfx3d.pendingState.clearDepth = DS_DEPTH15TO24(0x7FFF);
	gfx3d.pendingState.clearColor = 0;
	gfx3d.pendingState.fogColor = 0;
	gfx3d.pendingState.fogOffset = 0;
	gfx3d.pendingState.fogShift = 0;
}

u32 GFX3D_GetRender3DFrameCount()
{
	return gfx3d.render3DFrameCount;
}

void GFX3D_ResetRender3DFrameCount()
{
	gfx3d.render3DFrameCount = 0;
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
static T GFX3D_LerpFloat(const float ratio, const T& x0, const T& x1)
{
	return (T)((float)x0 + ((float)(x1 - x0) * ratio));
}

template <typename T>
static T GFX3D_LerpSigned(const s64 ratio_20_12, const s64 x0, const s64 x1)
{
	return (T)( ((x0 * 4096) + ((x1 - x0) * ratio_20_12)) / 4096 );
}

template <typename T>
static T GFX3D_LerpUnsigned(const u64 ratio_20_12, const u64 x0, const u64 x1)
{
	return (T)( ((x0 << 12) + ((x1 - x0) * ratio_20_12)) >> 12 );
}

//http://www.cs.berkeley.edu/~ug/slide/pipeline/assignments/as6/discussion.shtml
template <ClipperMode CLIPPERMODE, int COORD, int WHICH>
static FORCEINLINE void GFX3D_ClipPoint(const NDSVertex &insideVtx, const NDSVertex &outsideVtx, NDSVertex &outClippedVtx)
{
	const s64 coord_inside  = insideVtx.position.coord[COORD];
	const s64 coord_outside = outsideVtx.position.coord[COORD];
	const s64 w_inside  = (WHICH == -1) ? -insideVtx.position.coord[3]  : insideVtx.position.coord[3];
	const s64 w_outside = (WHICH == -1) ? -outsideVtx.position.coord[3] : outsideVtx.position.coord[3];
	
	// Calculating the ratio as-is would result in dividing a 20.12 numerator by a 20.12
	// denominator, thus stripping the quotient of its fractional component. We want to
	// retain the fractional component for this calculation, so we shift the numerator up
	// 16 bits to 36.28 so that the quotient becomes 24.16. Finally, we shift down 4 bits
	// to make the final value 20.12.
	const s64 t_s64 = ( ((coord_inside * 65536) - (w_inside * 65536)) / ((w_outside-w_inside) - (coord_outside-coord_inside)) ) / 16;
	const u64 t_u64 = (u64)t_s64; // Note: We are assuming that the ratio will never be negative. If it is... then oops!
	
	outClippedVtx.position.x = GFX3D_LerpSigned<s32>(t_s64, insideVtx.position.x, outsideVtx.position.x);
	outClippedVtx.position.y = GFX3D_LerpSigned<s32>(t_s64, insideVtx.position.y, outsideVtx.position.y);
	outClippedVtx.position.z = GFX3D_LerpSigned<s32>(t_s64, insideVtx.position.z, outsideVtx.position.z);
	outClippedVtx.position.w = GFX3D_LerpSigned<s32>(t_s64, insideVtx.position.w, outsideVtx.position.w);
	
	switch (CLIPPERMODE)
	{
		case ClipperMode_Full:
			outClippedVtx.texCoord.u = GFX3D_LerpSigned<s32>(t_s64, insideVtx.texCoord.s, outsideVtx.texCoord.s);
			outClippedVtx.texCoord.v = GFX3D_LerpSigned<s32>(t_s64, insideVtx.texCoord.t, outsideVtx.texCoord.t);
			outClippedVtx.color.r = GFX3D_LerpUnsigned<u8>(t_u64, insideVtx.color.r, outsideVtx.color.r);
			outClippedVtx.color.g = GFX3D_LerpUnsigned<u8>(t_u64, insideVtx.color.g, outsideVtx.color.g);
			outClippedVtx.color.b = GFX3D_LerpUnsigned<u8>(t_u64, insideVtx.color.b, outsideVtx.color.b);
			break;
			
		case ClipperMode_FullColorInterpolate:
			outClippedVtx.texCoord.u = GFX3D_LerpSigned<s32>(t_s64, insideVtx.texCoord.s, outsideVtx.texCoord.s);
			outClippedVtx.texCoord.v = GFX3D_LerpSigned<s32>(t_s64, insideVtx.texCoord.t, outsideVtx.texCoord.t);
			outClippedVtx.color.r = GFX3D_LerpUnsigned<u8>(t_u64, insideVtx.color.r, outsideVtx.color.r);
			outClippedVtx.color.g = GFX3D_LerpUnsigned<u8>(t_u64, insideVtx.color.g, outsideVtx.color.g);
			outClippedVtx.color.b = GFX3D_LerpUnsigned<u8>(t_u64, insideVtx.color.b, outsideVtx.color.b);
			break;
			
		case ClipperMode_DetermineClipOnly:
			// Do nothing.
			break;
	}
	
	//this seems like a prudent measure to make sure that math doesnt make a point pop back out
	//of the clip volume through interpolation
	if (WHICH == -1)
	{
		outClippedVtx.position.coord[COORD] = -outClippedVtx.position.coord[3];
	}
	else
	{
		outClippedVtx.position.coord[COORD] = outClippedVtx.position.coord[3];
	}
}

#define MAX_SCRATCH_CLIP_VERTS (4*6 + 40)
static NDSVertex scratchClipVerts[MAX_SCRATCH_CLIP_VERTS];
static size_t numScratchClipVerts = 0;

template <ClipperMode CLIPPERMODE, int COORD, int WHICH, class NEXT>
class ClipperPlane
{
public:
	ClipperPlane(NEXT &next) : m_next(next) {}
	
	void init(NDSVertex *vtxList)
	{
		m_prevVert =  NULL;
		m_firstVert = NULL;
		m_next.init(vtxList);
	}
	
	void clipVert(const NDSVertex &vtx)
	{
		if (m_prevVert != NULL)
		{
			this->clipSegmentVsPlane(*m_prevVert, vtx);
		}
		else
		{
			m_firstVert = (NDSVertex *)&vtx;
		}
		
		m_prevVert = (NDSVertex *)&vtx;
	}

	// closes the loop and returns the number of clipped output verts
	int finish()
	{
		if (m_prevVert != NULL)
		{
			this->clipVert(*m_firstVert);
		}
		
		return m_next.finish();
	}

private:
	NDSVertex *m_prevVert;
	NDSVertex *m_firstVert;
	NEXT &m_next;
	
	FORCEINLINE void clipSegmentVsPlane(const NDSVertex &vtx0, const NDSVertex &vtx1)
	{
		const bool out0 = (WHICH == -1) ? (vtx0.position.coord[COORD] < -vtx0.position.coord[3]) : (vtx0.position.coord[COORD] > vtx0.position.coord[3]);
		const bool out1 = (WHICH == -1) ? (vtx1.position.coord[COORD] < -vtx1.position.coord[3]) : (vtx1.position.coord[COORD] > vtx1.position.coord[3]);
		
		//CONSIDER: should we try and clip things behind the eye? does this code even successfully do it? not sure.
		//if (COORD==2 && WHICH==1) {
		//	out0 = vtx0.position.coord[2] < 0;
		//	out1 = vtx1.position.coord[2] < 0;
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
			m_next.clipVert(vtx1);
		}
		
		//exiting volume: insert the clipped point
		if (!out0 && out1)
		{
			CLIPLOG(" exiting\n");
			assert((u32)numScratchClipVerts < MAX_SCRATCH_CLIP_VERTS);
			GFX3D_ClipPoint<CLIPPERMODE, COORD, WHICH>(vtx0, vtx1, scratchClipVerts[numScratchClipVerts]);
			m_next.clipVert(scratchClipVerts[numScratchClipVerts++]);
		}
		
		//entering volume: insert clipped point and the next (interior) point
		if (out0 && !out1)
		{
			CLIPLOG(" entering\n");
			assert((u32)numScratchClipVerts < MAX_SCRATCH_CLIP_VERTS);
			GFX3D_ClipPoint<CLIPPERMODE, COORD, WHICH>(vtx1, vtx0, scratchClipVerts[numScratchClipVerts]);
			m_next.clipVert(scratchClipVerts[numScratchClipVerts++]);
			m_next.clipVert(vtx1);
		}
	}
};

class ClipperOutput
{
public:
	void init(NDSVertex *vtxList)
	{
		m_nextDestVert = vtxList;
		m_numVerts = 0;
	}
	
	void clipVert(const NDSVertex &vtx)
	{
		assert((u32)m_numVerts < MAX_CLIPPED_VERTS);
		*m_nextDestVert++ = vtx;
		m_numVerts++;
	}
	
	int finish()
	{
		return m_numVerts;
	}
	
private:
	NDSVertex *m_nextDestVert;
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
PolygonType GFX3D_GenerateClippedPoly(const u16 rawPolyIndex, const PolygonType rawPolyType, const NDSVertex *(&rawVtx)[4], CPoly &outCPoly)
{
	CLIPLOG("==Begin poly==\n");
	
	PolygonType outClippedType;
	numScratchClipVerts = 0;
	
	switch (CLIPPERMODE)
	{
		case ClipperMode_Full:
		{
			clipper1.init(outCPoly.vtx);
			for (size_t i = 0; i < (size_t)rawPolyType; i++)
				clipper1.clipVert(*rawVtx[i]);
			
			outClippedType = (PolygonType)clipper1.finish();
			break;
		}
			
		case ClipperMode_FullColorInterpolate:
		{
			clipper1i.init(outCPoly.vtx);
			for (size_t i = 0; i < (size_t)rawPolyType; i++)
				clipper1i.clipVert(*rawVtx[i]);
			
			outClippedType = (PolygonType)clipper1i.finish();
			break;
		}
			
		case ClipperMode_DetermineClipOnly:
		{
			clipper1d.init(outCPoly.vtx);
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

