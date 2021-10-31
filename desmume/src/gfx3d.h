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

#ifndef _GFX3D_H_
#define _GFX3D_H_

#include <iosfwd>
#include <ostream>
#include <istream>

#include "types.h"
#include "matrix.h"
#include "GPU.h"

class EMUFILE;

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

#define GFX3D_5TO6(x) ((x)?(((x)<<1)+1):0)
#define GFX3D_5TO6_LOOKUP(x) (material_5bit_to_6bit[(x)])

// 15-bit to 24-bit depth formula from http://nocash.emubase.de/gbatek.htm#ds3drearplane
extern CACHE_ALIGN u32 dsDepthExtend_15bit_to_24bit[32768];
#define DS_DEPTH15TO24(depth) ( dsDepthExtend_15bit_to_24bit[(depth) & 0x7FFF] )

extern CACHE_ALIGN MatrixStack<MATRIXMODE_PROJECTION> mtxStackProjection;
extern CACHE_ALIGN MatrixStack<MATRIXMODE_POSITION> mtxStackPosition;
extern CACHE_ALIGN MatrixStack<MATRIXMODE_POSITION_VECTOR> mtxStackPositionVector;
extern CACHE_ALIGN MatrixStack<MATRIXMODE_TEXTURE> mtxStackTexture;

// POLYGON PRIMITIVE TYPES
enum PolygonPrimitiveType
{
	GFX3D_TRIANGLES				= 0,
	GFX3D_QUADS					   = 1,
	GFX3D_TRIANGLE_STRIP		   = 2,
	GFX3D_QUAD_STRIP			   = 3,
	GFX3D_TRIANGLES_LINE		   = 4,
	GFX3D_QUADS_LINE			   = 5,
	GFX3D_TRIANGLE_STRIP_LINE	= 6,
	GFX3D_QUAD_STRIP_LINE		= 7
};

// POLYGON MODES
enum PolygonMode
{
	POLYGON_MODE_MODULATE		= 0,
	POLYGON_MODE_DECAL			= 1,
	POLYGON_MODE_TOONHIGHLIGHT	= 2,
	POLYGON_MODE_SHADOW			= 3
};

// POLYGON TYPES
enum PolygonType
{
	POLYGON_TYPE_TRIANGLE	= 3,
	POLYGON_TYPE_QUAD		   = 4
};

// TEXTURE PARAMETERS - FORMAT ID
enum NDSTextureFormat
{
	TEXMODE_NONE								= 0,
	TEXMODE_A3I5								= 1,
	TEXMODE_I2									= 2,
	TEXMODE_I4									= 3,
	TEXMODE_I8									= 4,
	TEXMODE_4X4									= 5,
	TEXMODE_A5I3								= 6,
	TEXMODE_16BPP								= 7
};

enum TextureTransformationMode
{
	TextureTransformationMode_None				= 0,
	TextureTransformationMode_TexCoordSource	= 1,
	TextureTransformationMode_NormalSource		= 2,
	TextureTransformationMode_VertexSource		= 3
};

enum PolygonShadingMode
{
	PolygonShadingMode_Toon						= 0,
	PolygonShadingMode_Highlight				= 1
};

void gfx3d_init();
void gfx3d_deinit();
void gfx3d_reset();

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u8 Light0:1;						//     0: Light 0; 0=Disable, 1=Enable
		u8 Light1:1;						//     1: Light 1; 0=Disable, 1=Enable
		u8 Light2:1;						//     2: Light 2; 0=Disable, 1=Enable
		u8 Light3:1;						//     3: Light 3; 0=Disable, 1=Enable
		u8 Mode:2;							//  4- 5: Polygon mode;
											//        0=Modulate
											//        1=Decal
											//        2=Toon/Highlight
											//        3=Shadow
		u8 BackSurface:1;					//     6: Back surface; 0=Hide, 1=Render
		u8 FrontSurface:1;					//     7: Front surface; 0=Hide, 1=Render
		
		u8 :3;								//  8-10: Unused bits
		u8 TranslucentDepthWrite_Enable:1;	//    11: Translucent depth write; 0=Keep 1=Replace
		u8 FarPlaneIntersect_Enable:1;		//    12: Far-plane intersecting polygons; 0=Hide, 1=Render/clipped
		u8 OneDotPolygons_Enable:1;			//    13: One-dot polygons; 0=Hide, 1=Render
		u8 DepthEqualTest_Enable:1;			//    14: Depth test mode; 0=Less, 1=Equal
		u8 Fog_Enable:1;					//    15: Fog; 0=Disable, 1=Enable
		
		u8 Alpha:5;							// 16-20: Alpha value
		u8 :3;								// 21-23: Unused bits
		
		u8 PolygonID:6;						// 24-29: Polygon ID
		u8 :2;								// 30-31: Unused bits
#else
		u8 :2;								// 30-31: Unused bits
		u8 PolygonID:6;						// 24-29: Polygon ID
		
		u8 :3;								// 21-23: Unused bits
		u8 Alpha:5;							// 16-20: Alpha value
		
		u8 Fog_Enable:1;					//    15: Fog; 0=Disable, 1=Enable
		u8 DepthEqualTest_Enable:1;			//    14: Depth test mode; 0=Less, 1=Equal
		u8 OneDotPolygons_Enable:1;			//    13: One-dot polygons; 0=Hide, 1=Render
		u8 FarPlaneIntersect_Enable:1;		//    12: Far-plane intersecting polygons; 0=Hide, 1=Render/clipped
		u8 TranslucentDepthWrite_Enable:1;	//    11: Translucent depth write; 0=Keep 1=Replace
		u8 :3;								//  8-10: Unused bits
		
		u8 FrontSurface:1;					//     7: Front surface; 0=Hide, 1=Render
		u8 BackSurface:1;					//     6: Back surface; 0=Hide, 1=Render
		u8 Mode:2;							//  4- 5: Polygon mode;
											//        0=Modulate
											//        1=Decal
											//        2=Toon/Highlight
											//        3=Shadow
		u8 Light3:1;						//     3: Light 3; 0=Disable, 1=Enable
		u8 Light2:1;						//     2: Light 2; 0=Disable, 1=Enable
		u8 Light1:1;						//     1: Light 1; 0=Disable, 1=Enable
		u8 Light0:1;						//     0: Light 0; 0=Disable, 1=Enable
#endif
	};
	
	struct
	{
#ifndef MSB_FIRST
		u8 LightMask:4;						//  0- 3: Light enable mask
		u8 :2;
		u8 SurfaceCullingMode:2;			//  6- 7: Surface culling mode;
											//        0=Cull front and back
											//        1=Cull front
											//        2=Cull back
											//        3=No culling
		u8 :8;
		u8 :8;
		u8 :8;
#else
		u8 :8;
		u8 :8;
		u8 :8;
		
		u8 SurfaceCullingMode:2;			//  6- 7: Surface culling mode;
											//        0=Cull front and back
											//        1=Cull front
											//        2=Cull back
											//        3=No culling
		u8 :2;
		u8 LightMask:4;						//  0- 3: Light enable mask
#endif
	};
} POLYGON_ATTR;

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u16 VRAMOffset:16;					//  0-15: VRAM offset address
		
		u16 RepeatS_Enable:1;				//    16: Repeat for S-coordinate; 0=Clamp 1=Repeat
		u16 RepeatT_Enable:1;				//    17: Repeat for T-coordinate; 0=Clamp 1=Repeat
		u16 MirroredRepeatS_Enable:1;		//    18: Mirrored repeat for S-coordinate, interacts with bit 16; 0=Disable 1=Enable
		u16 MirroredRepeatT_Enable:1;		//    19: Mirrored repeat for T-coordinate, interacts with bit 17; 0=Disable 1=Enable
		u16 SizeShiftS:3;					// 20-22: Texel size shift for S-coordinate; 0...7, where the actual texel size is (8 << N)
		u16 SizeShiftT:3;					// 23-25: Texel size shift for T-coordinate; 0...7, where the actual texel size is (8 << N)
		u16 PackedFormat:3;					// 26-28: Packed texture format;
											//        0=None
											//        1=A3I5, 5-bit indexed color (32-color palette) with 3-bit alpha (0...7, where 0=Fully Transparent and 7=Opaque)
											//        2=I2, 2-bit indexed color (4-color palette)
											//        3=I4, 4-bit indexed color (16-color palette)
											//        4=I8, 8-bit indexed color (256-color palette)
											//        5=4x4-texel compressed
											//        6=A5I3, 3-bit indexed color (8-color palette) with 5-bit alpha (0...31, where 0=Fully Transparent and 31=Opaque)
											//        7=Direct 16-bit color
		u16 KeyColor0_Enable:1;				//    29: Use palette color 0 as transparent; 0=Displayed 1=Transparent
		u16 TexCoordTransformMode:2;		// 30-31: Texture coordinate transformation mode;
											//        0=No transformation
											//        1=TexCoord source
											//        2=Normal source
											//        3=Vertex source
#else
		u16 TexCoordTransformMode:2;		// 30-31: Texture coordinate transformation mode;
											//        0=No transformation
											//        1=TexCoord source
											//        2=Normal source
											//        3=Vertex source
		u16 KeyColor0_Enable:1;				//    29: Use palette color 0 as transparent; 0=Displayed 1=Transparent
		u16 PackedFormat:3;					// 26-28: Packed texture format;
											//        0=None
											//        1=A3I5, 5-bit indexed color (32-color palette) with 3-bit alpha (0...7, where 0=Fully Transparent and 7=Opaque)
											//        2=I2, 2-bit indexed color (4-color palette)
											//        3=I4, 4-bit indexed color (16-color palette)
											//        4=I8, 8-bit indexed color (256-color palette)
											//        5=4x4-texel compressed
											//        6=A5I3, 3-bit indexed color (8-color palette) with 5-bit alpha (0...31, where 0=Fully Transparent and 31=Opaque)
											//        7=Direct 16-bit color
		u16 SizeShiftT:3;					// 23-25: Texel size shift for T-coordinate; 0...7, where the actual texel size is (8 << N)
		u16 SizeShiftS:3;					// 20-22: Texel size shift for S-coordinate; 0...7, where the actual texel size is (8 << N)
		u16 MirroredRepeatT_Enable:1;		//    19: Mirrored repeat for T-coordinate, interacts with bit 17; 0=Disable 1=Enable
		u16 MirroredRepeatS_Enable:1;		//    18: Mirrored repeat for S-coordinate, interacts with bit 16; 0=Disable 1=Enable
		u16 RepeatT_Enable:1;				//    17: Repeat for T-coordinate; 0=Clamp 1=Repeat
		u16 RepeatS_Enable:1;				//    16: Repeat for S-coordinate; 0=Clamp 1=Repeat
		
		u16 VRAMOffset:16;					//  0-15: VRAM offset address
#endif
	};
	
	struct
	{
#ifndef MSB_FIRST
		u16 :16;
		u16 TextureWrapMode:4;				// 16-19: Texture wrap mode for repeat and mirrored repeat
		u16 :12;
#else
		u16 :12;
		u16 TextureWrapMode:4;				// 16-19: Texture wrap mode for repeat and mirrored repeat
		u16 :16;
#endif
	};
} TEXIMAGE_PARAM;

struct POLY {
	PolygonType type; //tri or quad
	PolygonPrimitiveType vtxFormat;
	u16 vertIndexes[4]; //up to four verts can be referenced by this poly
	POLYGON_ATTR attribute;
	TEXIMAGE_PARAM texParam;
	u32 texPalette; //the hardware rendering params
	u32 viewport;
	float miny, maxy;

	void setVertIndexes(int a, int b, int c, int d=-1)
	{
		vertIndexes[0] = a;
		vertIndexes[1] = b;
		vertIndexes[2] = c;
		if(d != -1) { vertIndexes[3] = d; type = POLYGON_TYPE_QUAD; }
		else type = POLYGON_TYPE_TRIANGLE;
	}
	
	bool isWireframe() const
	{
		return (this->attribute.Alpha == 0);
	}
	
	bool isOpaque() const
	{
		return (this->attribute.Alpha == 31);
	}
	
	bool isTranslucent() const
	{
		// First, check if the polygon is wireframe or opaque.
		// If neither, then it must be translucent.
		if (!this->isWireframe() && !this->isOpaque())
		{
			return true;
		}
		
		// Also check for translucent texture format.
		const NDSTextureFormat texFormat = (NDSTextureFormat)this->texParam.PackedFormat;
		const PolygonMode mode = (PolygonMode)this->attribute.Mode;
		
		//a5i3 or a3i5 -> translucent
		if ( (texFormat == TEXMODE_A3I5 || texFormat == TEXMODE_A5I3) && (mode != POLYGON_MODE_DECAL && mode != POLYGON_MODE_SHADOW) )
		{
			return true;
		}
		
		return false;
	}
	
	void save(EMUFILE &os);
	void load(EMUFILE &is);
};

#define POLYLIST_SIZE 20000
#define VERTLIST_SIZE (POLYLIST_SIZE * 4)

struct POLYLIST {
	POLY list[POLYLIST_SIZE];
	size_t count;
	size_t opaqueCount;
};

//just a vert with a 4 float position
struct VERT_POS4f
{
	union {
		float coord[4];
		struct {
			float x,y,z,w;
		};
		struct {
			float x,y,z,w;
		} position;
	};
	void set_coord(float x, float y, float z, float w)
	{ 
		this->x = x; 
		this->y = y; 
		this->z = z; 
		this->w = w; 
	}
};

#include "PACKED.h"

// This struct is padded in such a way so that each component can be accessed with a 16-byte alignment.
struct VERT
{
	union
	{
		float coord[4];
		struct
		{
			float x, y, z, w;
		};
	};
	
	union
	{
		float texcoord[4];
		struct
		{
			float u, v, tcPad2, tcPad3;
		};
	};
	
	union
	{
		float fcolor[4];
		struct
		{
			float rf, gf, bf, af; // The alpha value is unused and only exists for padding purposes.
		};
	};
	
	union
	{
		u32 color32;
		u8 color[4];
		
		struct
		{
			u8 r, g, b, a; // The alpha value is unused and only exists for padding purposes.
		};
	};
	
	u8 padFinal[12]; // Final padding to bring the struct to exactly 64 bytes.
	
	void set_coord(float x, float y, float z, float w)
	{
		this->x = x; 
		this->y = y; 
		this->z = z; 
		this->w = w; 
	}
	
	void set_coord(float* coords)
	{
		x = coords[0];
		y = coords[1];
		z = coords[2];
		w = coords[3];
	}
	
	void color_to_float()
	{
		rf = (float)r;
		gf = (float)g;
		bf = (float)b;
		af = (float)a;
	}
	
	void save(EMUFILE &os);
	void load(EMUFILE &is);
};

#include "PACKED_END.h"

#define INDEXLIST_SIZE (POLYLIST_SIZE * 4)
struct INDEXLIST {
	int list[INDEXLIST_SIZE];
};

struct VIEWPORT {
	u8 x, y;
	u16 width, height;
	void decode(u32 v);
};

//ok, imagine the plane that cuts diagonally across a cube such that it clips
//out to be a hexagon. within that plane, draw a quad such that it cuts off
//four corners of the hexagon, and you will observe a decagon
#define MAX_CLIPPED_VERTS 10

enum ClipperMode
{
	ClipperMode_DetermineClipOnly = 0,		// Retains only the pointer to the original polygon info. All other information in CPoly is considered undefined.
	ClipperMode_Full = 1,					// Retains all of the modified polygon's info in CPoly, including the clipped vertex info.
	ClipperMode_FullColorInterpolate = 2	// Same as ClipperMode_Full, but the vertex color attribute is better interpolated.
};

struct CPoly
{
	u16 index; // The index number of this polygon in the full polygon list.
	PolygonType type; //otherwise known as "count" of verts
	POLY *poly;
	VERT clipVerts[MAX_CLIPPED_VERTS];
};

class GFX3D_Clipper
{
protected:
	size_t _clippedPolyCounter;
	CPoly *_clippedPolyList; // The output of clipping operations goes into here. Be sure you init it before clipping!
	
public:
	GFX3D_Clipper();
	
	const CPoly* GetClippedPolyBufferPtr();
	void SetClippedPolyBufferPtr(CPoly *bufferPtr);
	
	const CPoly& GetClippedPolyByIndex(size_t index) const;
	size_t GetPolyCount() const;
	
	void Reset();
	template<ClipperMode CLIPPERMODE> bool ClipPoly(const u16 polyIndex, const POLY &poly, const VERT **verts); // the entry point for poly clipping
};

//used to communicate state to the renderer
struct GFX3D_State
{
	GFX3D_State()
		: enableTexturing(true)
		, enableAlphaTest(true)
		, enableAlphaBlending(true)
		, enableAntialiasing(false)
		, enableEdgeMarking(false)
		, enableClearImage(false)
		, enableFog(false)
		, enableFogAlphaOnly(false)
		, shading(PolygonShadingMode_Toon)
		, alphaTestRef(0)
		, activeFlushCommand(0)
		, pendingFlushCommand(0)
		, clearDepth(DS_DEPTH15TO24(0x7FFF))
		, clearColor(0)
		, fogColor(0)
		, fogOffset(0)
		, fogShift(0)
		, invalidateToon(true)
	{
		for(u32 i=0;i<ARRAY_SIZE(shininessTable);i++)
			shininessTable[i] = 0;

		for(u32 i=0;i<ARRAY_SIZE(u16ToonTable);i++)
			u16ToonTable[i] = 0;
	}

	IOREG_DISP3DCNT savedDISP3DCNT;
	
	BOOL enableTexturing, enableAlphaTest, enableAlphaBlending, 
		enableAntialiasing, enableEdgeMarking, enableClearImage, enableFog, enableFogAlphaOnly;

	u32 shading;

	BOOL wbuffer, sortmode;
	u8 alphaTestRef;
	u32 activeFlushCommand;
	u32 pendingFlushCommand;

	u32 clearDepth;
	u32 clearColor;
	#include "PACKED.h"
	struct {
		u32 fogColor;
		u32 pad[3]; //for savestate compatibility as of 26-jul-09
	};
	#include "PACKED_END.h"
	u32 fogOffset;
	u32 fogShift;

	bool invalidateToon;
	CACHE_ALIGN u16 u16ToonTable[32];
	u8 shininessTable[128];
	u8 *fogDensityTable;		// Alias to MMU.ARM9_REG+0x0360
	u16 *edgeMarkColorTable;	// Alias to MMU.ARM9_REG+0x0330
};

struct Viewer3d_State
{
	int frameNumber;
	GFX3D_State state;
	VERT vertList[VERTLIST_SIZE];
	POLYLIST polylist;
	INDEXLIST indexlist;
	
	size_t vertListCount;
};

extern Viewer3d_State* viewer3d_state;

struct GFX3D
{
	GFX3D()
		: polylist(NULL)
		, vertList(NULL)
		, render3DFrameCount(0) {
	}

	//currently set values
	GFX3D_State state;

	//values used for the currently-rendered frame (committed with each flush)
	GFX3D_State renderState;

	POLYLIST *polylist;
	VERT *vertList;
	INDEXLIST indexlist;
	
	size_t clippedPolyCount;
	size_t clippedPolyOpaqueCount;
	CPoly *clippedPolyList;
	
	size_t vertListCount;
	u32 render3DFrameCount;			// Increments when gfx3d_doFlush() is called. Resets every 60 video frames.
};
extern GFX3D gfx3d;

//---------------------

extern CACHE_ALIGN u32 dsDepthExtend_15bit_to_24bit[32768];
extern CACHE_ALIGN u8 mixTable555[32][32][32];

extern u32 isSwapBuffers;

void gfx3d_glFlush(u32 v);
// end GE commands

void gfx3d_glFogColor(u32 v);
void gfx3d_glFogOffset (u32 v);
void gfx3d_glClearDepth(u32 v);
void gfx3d_glSwapScreen(u32 screen);
int gfx3d_GetNumPolys();
int gfx3d_GetNumVertex();
void gfx3d_UpdateToonTable(u8 offset, u16 val);
void gfx3d_UpdateToonTable(u8 offset, u32 val);
s32 gfx3d_GetClipMatrix (const u32 index);
s32 gfx3d_GetDirectionalMatrix(const u32 index);
void gfx3d_glAlphaFunc(u32 v);
u32 gfx3d_glGetPosRes(const size_t index);
u16 gfx3d_glGetVecRes(const size_t index);
void gfx3d_VBlankSignal();
void gfx3d_VBlankEndSignal(bool skipFrame);
void gfx3d_execute3D();
void gfx3d_sendCommandToFIFO(u32 val);
void gfx3d_sendCommand(u32 cmd, u32 param);

//other misc stuff
template<MatrixMode MODE> void gfx3d_glGetMatrix(const int index, float (&dst)[16]);
void gfx3d_glGetLightDirection(const size_t index, u32 &dst);
void gfx3d_glGetLightColor(const size_t index, u32 &dst);

struct SFORMAT;
extern SFORMAT SF_GFX3D[];
void gfx3d_PrepareSaveStateBufferWrite();
void gfx3d_savestate(EMUFILE &os);
bool gfx3d_loadstate(EMUFILE &is, int size);
void gfx3d_FinishLoadStateBufferRead();

void gfx3d_ClearStack();

void gfx3d_parseCurrentDISP3DCNT();
void ParseReg_DISP3DCNT();

#endif //_GFX3D_H_
