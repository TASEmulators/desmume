/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2008-2024 DeSmuME team

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
	POLYGON_TYPE_UNDEFINED		= 0,
	POLYGON_TYPE_TRIANGLE		= 3,
	POLYGON_TYPE_QUAD			= 4
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
	u16 value;
	
	struct
	{
#ifndef MSB_FIRST
		u8 XOffset;
		u8 YOffset;
#else
		u8 YOffset;
		u8 XOffset;
#endif
	};
} IOREG_CLRIMAGE_OFFSET;

typedef union
{
	u8 cmd[4];								//  0- 7: Unpacked command OR packed command #1
											//  8-15: Packed command #2
											// 16-23: Packed command #3
											// 24-31: Packed command #4
	
	u32 command;							// 8-bit unpacked command
	u32 param;								// Parameter(s) for previous command(s)
	
} IOREG_GXFIFO;								// 0x04000400: Geometry command/parameter sent to FIFO

typedef union
{
#ifndef MSB_FIRST
		u8 MtxMode:2;						//  0- 1: Set matrix mode;
											//        0=Projection
											//        1=Position
											//        2=Position+Vector
											//        3=Texture
		u8 :6;								//  2- 7: Unused bits
		
		u32 :24;							//  8-31: Unused bits
#else
		u32 :24;							//  8-31: Unused bits
		
		u8 :6;								//  2- 7: Unused bits
		u8 MtxMode:2;						//  0- 1: Set matrix mode;
											//        0=Projection
											//        1=Position
											//        2=Position+Vector
											//        3=Texture
#endif
} IOREG_MTX_MODE;							// 0x04000440: MTX_MODE command port

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
} POLYGON_ATTR;								// 0x040004A4: POLYGON_ATTR command port

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
} TEXIMAGE_PARAM;							// 0x040004A8: TEXIMAGE_PARAM command port

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u8 YSortMode:1;						//     0: Translucent polygon Y-sorting mode; 0=Auto-sort, 1=Manual-sort
		u8 DepthMode:1;						//     1: Depth buffering select; 0=Z 1=W
		u8 :6;								//  2- 7: Unused bits
		
		u32 :24;							//  8-31: Unused bits
#else
		u32 :24;							//  8-31: Unused bits
		
		u8 :6;								//  2- 7: Unused bits
		u8 DepthMode:1;						//     1: Depth buffering select; 0=Z 1=W
		u8 YSortMode:1;						//     0: Translucent polygon Y-sorting mode; 0=Auto-sort, 1=Manual-sort
#endif
	};
} IOREG_SWAP_BUFFERS;						// 0x04000540: SWAP_BUFFERS command port

typedef union
{
	u32 value;
	
	struct
	{
		// Coordinate (0,0) represents the bottom-left of the screen.
		// Coordinate (255,191) represents the top-right of the screen.
		
#ifndef MSB_FIRST
		u8 X1;								//  0- 7: First X-coordinate; 0...255
		u8 Y1;								//  8-15: First Y-coordinate; 0...191
		u8 X2;								// 16-23: Second X-coordinate; 0...255
		u8 Y2;								// 24-31: Second Y-coordinate; 0...191
#else
		u8 Y2;								// 24-31: Second Y-coordinate; 0...191
		u8 X2;								// 16-23: Second X-coordinate; 0...255
		u8 Y1;								//  8-15: First Y-coordinate; 0...191
		u8 X1;								//  0- 7: First X-coordinate; 0...255
#endif
	};
} IOREG_VIEWPORT;							// 0x04000580: VIEWPORT command port

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u8 TestBusy:1;
		u8 BoxTestResult:1;
		u8 :6;
		
		u8 PosVecMtxStackLevel:5;
		u8 ProjMtxStackLevel:1;
		u8 MtxStackBusy:1;
		u8 AckMtxStackError:1;
		
		u16 CommandListCount:9;
		u8 CommandListLessThanHalf:1;
		u8 CommandListEmpty:1;
		u8 EngineBusy:1;
		u8 :2;
		u8 CommandListIRQ:2;
#else
		u8 :6;
		u8 BoxTestResult:1;
		u8 TestBusy:1;
		
		u8 AckMtxStackError:1;
		u8 MtxStackBusy:1;
		u8 ProjMtxStackLevel:1;
		u8 PosVecMtxStackLevel:5;
		
		u8 CommandListIRQ:2;
		u8 :2;
		u8 EngineBusy:1;
		u8 CommandListEmpty:1;
		u8 CommandListLessThanHalf:1;
		u16 CommandListCount:9;
#endif
	};
	
} IOREG_GXSTAT;								// 0x04000600: Geometry engine status

typedef union
{
	u32 value;
	
	struct
	{
		u16 PolygonCount;					//  0-15: Number of polygons currently stored in polygon list RAM; 0...2048
		u16 VertexCount;					// 16-31: Number of vertices currently stored in vertex RAM; 0...6144
	};
} IOREG_RAM_COUNT;							// 0x04000604: Polygon list and vertex RAM count

struct GFX3D_IOREG
{
	u8 RDLINES_COUNT;						// 0x04000320
	u8 __unused1[15];
	u16 EDGE_COLOR[8];						// 0x04000330
	u8 ALPHA_TEST_REF;						// 0x04000340
	u8 __unused2[15];
	u32 CLEAR_COLOR;						// 0x04000350
	u16 CLEAR_DEPTH;						// 0x04000354
	IOREG_CLRIMAGE_OFFSET CLRIMAGE_OFFSET;	// 0x04000356
	u32 FOG_COLOR;							// 0x04000358
	u16 FOG_OFFSET;							// 0x0400035C
	u8 __unused3[2];
	u8 FOG_TABLE[32];						// 0x04000360
	u16 TOON_TABLE[32];						// 0x04000380
	u8 __unused4[64];
	
	IOREG_GXFIFO GXFIFO;					// 0x04000400
	u8 __unused5[60];
	
	// Geometry command ports
	u32 MTX_MODE;							// 0x04000440
	u32 MTX_PUSH;							// 0x04000444
	u32 MTX_POP;							// 0x04000448
	u32 MTX_STORE;							// 0x0400044C
	u32 MTX_RESTORE;						// 0x04000450
	u32 MTX_IDENTITY;						// 0x04000454
	u32 MTX_LOAD_4x4;						// 0x04000458
	u32 MTX_LOAD_4x3;						// 0x0400045C
	u32 MTX_MULT_4x4;						// 0x04000460
	u32 MTX_MULT_4x3;						// 0x04000464
	u32 MTX_MULT_3x3;						// 0x04000468
	u32 MTX_SCALE;							// 0x0400046C
	u32 MTX_TRANS;							// 0x04000470
	u8 __unused6[12];
	u32 COLOR;								// 0x04000480
	u32 NORMAL;								// 0x04000484
	u32 TEXCOORD;							// 0x04000488
	u32 VTX_16;								// 0x0400048C
	u32 VTX_10;								// 0x04000490
	u32 VTX_XY;								// 0x04000494
	u32 VTX_XZ;								// 0x04000498
	u32 VTX_YZ;								// 0x0400049C
	u32 VTX_DIFF;							// 0x040004A0
	u32 POLYGON_ATTR;						// 0x040004A4
	u32 TEXIMAGE_PARAM;						// 0x040004A8
	u32 PLTT_BASE;							// 0x040004AC
	u8 __unused7[16];
	u32 DIF_AMB;							// 0x040004C0
	u32 SPE_EMI;							// 0x040004C4
	u32 LIGHT_VECTOR;						// 0x040004C8
	u32 LIGHT_COLOR;						// 0x040004CC
	u32 SHININESS;							// 0x040004D0
	u8 __unused8[44];
	u32 BEGIN_VTXS;							// 0x04000500
	u32 END_VTXS;							// 0x04000504
	u8 __unused9[56];
	IOREG_SWAP_BUFFERS SWAP_BUFFERS;		// 0x04000540
	u8 __unused10[60];
	IOREG_VIEWPORT VIEWPORT;				// 0x04000580
	u8 __unused11[60];
	u32 BOX_TEST;							// 0x040005C0
	u32 POS_TEST;							// 0x040005C4
	u32 VEC_TEST;							// 0x040005C8
	u8 __unused12[52];
	
	IOREG_GXSTAT GXSTAT;					// 0x04000600
	IOREG_RAM_COUNT RAM_COUNT;				// 0x04000604
	u8 __unused13[8];
	u16 DISP_1DOT_DEPTH;					// 0x04000610
	u8 __unused14[14];
	u32 POS_RESULT[4];						// 0x04000620
	u16 VEC_RESULT[3];						// 0x04000630
	u8 __unused15[10];
	u8 CLIPMTX_RESULT[64];					// 0x04000640
	u8 VECMTX_RESULT[36];					// 0x04000680
};
typedef struct GFX3D_IOREG GFX3D_IOREG; // 0x04000320 - 0x040006A4

union GFX3D_Viewport
{
	u64 value;
	
	struct
	{
		s16 x;
		s16 y;
		u16 width;
		u16 height;
	};
};
typedef union GFX3D_Viewport GFX3D_Viewport;

struct POLY
{
	PolygonType type; //tri or quad
	PolygonPrimitiveType vtxFormat;
	u16 vertIndexes[4]; //up to four verts can be referenced by this poly
	
	POLYGON_ATTR attribute;
	TEXIMAGE_PARAM texParam;
	u32 texPalette; //the hardware rendering params
	GFX3D_Viewport viewport;
};
typedef struct POLY POLY;

// TODO: Handle these polygon utility functions in a class rather than as standalone functions.
// Most likely, the class will be some kind of polygon processing class, such as a polygon list
// handler or a polygon clipping handler. But before such a class is designed, simply handle
// these function here so that the POLY struct can remain as a POD struct.
bool GFX3D_IsPolyWireframe(const POLY &p);
bool GFX3D_IsPolyOpaque(const POLY &p);
bool GFX3D_IsPolyTranslucent(const POLY &p);

#define POLYLIST_SIZE 16384
#define CLIPPED_POLYLIST_SIZE (POLYLIST_SIZE * 2)
#define VERTLIST_SIZE (POLYLIST_SIZE * 4)

struct NDSVertex
{
	Vector4s32 position;
	Vector2s32 texCoord;
	Color4u8 color;
};
typedef struct NDSVertex NDSVertex;

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
	bool isPolyBackFacing;
	NDSVertex vtx[MAX_CLIPPED_VERTS];
};
typedef struct CPoly CPoly;

//used to communicate state to the renderer
struct GFX3D_State
{
	IOREG_DISP3DCNT DISP3DCNT;
	u8 fogShift;
	
	u8 alphaTestRef;
	u32 clearColor; // Not an RGBA8888 color. This uses its own packed format.
	u32 clearDepth;
	IOREG_CLRIMAGE_OFFSET clearImageOffset;
	u32 fogColor; // Not an RGBA8888 color. This uses its own packed format.
	u16 fogOffset;
	
	IOREG_SWAP_BUFFERS SWAP_BUFFERS;
	
	CACHE_ALIGN u16 edgeMarkColorTable[8];
	CACHE_ALIGN u8 fogDensityTable[32];
	CACHE_ALIGN u16 toonTable16[32];
};
typedef struct GFX3D_State GFX3D_State;

struct GFX3D_GeometryList
{
	PAGE_ALIGN NDSVertex rawVtxList[VERTLIST_SIZE];
	PAGE_ALIGN POLY rawPolyList[POLYLIST_SIZE];
	PAGE_ALIGN CPoly clippedPolyList[CLIPPED_POLYLIST_SIZE];
	
	size_t rawVertCount;
	size_t rawPolyCount;
	size_t clippedPolyCount;
	size_t clippedPolyOpaqueCount;
};
typedef struct GFX3D_GeometryList GFX3D_GeometryList;

struct GFX3D_State_LegacySave
{
	u32 enableTexturing;
	u32 enableAlphaTest;
	u32 enableAlphaBlending;
	u32 enableAntialiasing;
	u32 enableEdgeMarking;
	u32 enableClearImage;
	u32 enableFog;
	u32 enableFogAlphaOnly;
	
	u32 fogShift;
	
	u32 toonShadingMode;
	u32 enableWDepth;
	u32 polygonTransparentSortMode;
	u32 alphaTestRef;
	
	u32 clearColor;
	u32 clearDepth;
	
	u32 fogColor[4]; //for savestate compatibility as of 26-jul-09
	u32 fogOffset;
	
	u16 toonTable16[32];
	
	u32 activeFlushCommand;
	u32 pendingFlushCommand;
};
typedef struct GFX3D_State_LegacySave GFX3D_State_LegacySave;

struct GeometryEngineLegacySave
{
	u32 inBegin;
	TEXIMAGE_PARAM texParam;
	u32 texPalette;
	
	u32 mtxCurrentMode;
	CACHE_ALIGN NDSMatrix tempMultiplyMatrix;
	CACHE_ALIGN NDSMatrix currentMatrix[4];
	u8 mtxLoad4x4PendingIndex;
	u8 mtxLoad4x3PendingIndex;
	u8 mtxMultiply4x4TempIndex;
	u8 mtxMultiply4x3TempIndex;
	u8 mtxMultiply3x3TempIndex;
	
	Vector4s16 vtxPosition;
	u8 vtxPosition16CurrentIndex;
	u32 vtxFormat;
	
	Vector4s32 vecTranslate;
	u8 vecTranslateCurrentIndex;
	Vector4s32 vecScale;
	u8 vecScaleCurrentIndex;
	
	u32 texCoordT;
	u32 texCoordS;
	u32 texCoordTransformedT;
	u32 texCoordTransformedS;
	
	u32 boxTestCoordCurrentIndex;
	u32 positionTestCoordCurrentIndex;
	float positionTestVtxFloat[4]; // Historically, the position test vertices were stored as floating point values, not as integers.
	u16 boxTestCoord16[6];
	
	Color4u8 vtxColor;
	
	u32 regLightColor[4];
	u32 regLightDirection[4];
	u16 regDiffuse;
	u16 regAmbient;
	u16 regSpecular;
	u16 regEmission;
	
	u8 shininessTablePending[128];
	u8 shininessTableApplied[128];
	
	IOREG_VIEWPORT regViewport; // Historically, the viewport was stored as its raw register value.
	
	u8 shininessTablePendingIndex;
	
	u8 generateTriangleStripIndexToggle;
	u32 vtxCount;
	u32 vtxIndex[4];
	u32 isGeneratingFirstPolyOfStrip;
};
typedef struct GeometryEngineLegacySave GeometryEngineLegacySave;

struct GFX3D_LegacySave
{
	GFX3D_State_LegacySave statePending;
	GFX3D_State_LegacySave stateApplied;
	
	u32 clCommand; // Exists purely for save state compatibility, historically went unused since 09/20/2009.
	u32 clIndex; // Exists purely for save state compatibility, historically went unused since 09/20/2009.
	u32 clIndex2; // Exists purely for save state compatibility, historically went unused since 09/20/2009.
	u32 isSwapBuffersPending;
	u32 isDrawPending;
	
	IOREG_VIEWPORT rawPolyViewport[POLYLIST_SIZE]; // Historically, pending polygons kept a copy of the current viewport as a raw register value.
};
typedef struct GFX3D_LegacySave GFX3D_LegacySave;

struct Viewer3D_State
{
	int frameNumber;
	GFX3D_State state;
	GFX3D_GeometryList gList;
};
typedef struct Viewer3D_State Viewer3D_State;

extern Viewer3D_State viewer3D;

struct GFX3D
{
	GFX3D_State pendingState;
	GFX3D_State appliedState;
	GFX3D_GeometryList gList[2];
	
	u8 pendingListIndex;
	u8 appliedListIndex;
	bool isSwapBuffersPending;
	bool isDrawPending;

	POLYGON_ATTR regPolyAttrPending;
	POLYGON_ATTR regPolyAttrApplied;
	u32 render3DFrameCount; // Increments when gfx3d_doFlush() is called. Resets every 60 video frames.
	
	// Working lists for rendering.
	CACHE_ALIGN CPoly clippedPolyUnsortedList[CLIPPED_POLYLIST_SIZE]; // Records clipped polygon info on first pass
	CACHE_ALIGN u16 indexOfClippedPolyUnsortedList[CLIPPED_POLYLIST_SIZE];
	CACHE_ALIGN s64 rawPolySortYMin[POLYLIST_SIZE]; // Temp buffer used for processing polygon Y-sorting
	CACHE_ALIGN s64 rawPolySortYMax[POLYLIST_SIZE]; // Temp buffer used for processing polygon Y-sorting
	
	// Everything below is for save state compatibility.
	GFX3D_LegacySave legacySave;
	GeometryEngineLegacySave gEngineLegacySave;
	PAGE_ALIGN Color4u8 framebufferNativeSave[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT]; // Rendered 3D framebuffer that is saved in RGBA8888 color format at the native size.
};
typedef struct GFX3D GFX3D;

class NDSGeometryEngine
{
private:
	void __Init();
	
protected:
	CACHE_ALIGN NDSMatrix _mtxCurrent[4];
	CACHE_ALIGN NDSMatrix _pendingMtxLoad4x4;
	CACHE_ALIGN NDSMatrix _pendingMtxLoad4x3;
	CACHE_ALIGN NDSMatrix _tempMtxMultiply4x4;
	CACHE_ALIGN NDSMatrix _tempMtxMultiply4x3;
	CACHE_ALIGN NDSMatrix _tempMtxMultiply3x3;
	CACHE_ALIGN Vector4s32 _vecTranslate;
	CACHE_ALIGN Vector4s32 _vecScale;
	
	// Matrix stack handling
	CACHE_ALIGN NDSMatrixStack1  _mtxStackProjection;
	CACHE_ALIGN NDSMatrixStack32 _mtxStackPosition;
	CACHE_ALIGN NDSMatrixStack32 _mtxStackPositionVector;
	CACHE_ALIGN NDSMatrixStack1  _mtxStackTexture;
	
	CACHE_ALIGN Vector4s32 _vecNormal;
	CACHE_ALIGN Vector3s16 _vtxCoord16;
	CACHE_ALIGN Vector2s16 _texCoord16;
	CACHE_ALIGN Vector2s32 _texCoordTransformed;
	
	CACHE_ALIGN u8 _shininessTablePending[128];
	CACHE_ALIGN u8 _shininessTableApplied[128];
	
	MatrixMode _mtxCurrentMode;
	u8 _mtxStackIndex[4];
	u8 _mtxLoad4x4PendingIndex;
	u8 _mtxLoad4x3PendingIndex;
	u8 _mtxMultiply4x4TempIndex;
	u8 _mtxMultiply4x3TempIndex;
	u8 _mtxMultiply3x3TempIndex;
	u8 _vecScaleCurrentIndex;
	u8 _vecTranslateCurrentIndex;
	
	u32 _vtxColor15;
	Color4u8 _vtxColor555X;
	Color4u8 _vtxColor666X;
	
	bool _doesViewportNeedUpdate;
	bool _doesVertexColorNeedUpdate;
	bool _doesTransformedTexCoordsNeedUpdate;
	
	IOREG_VIEWPORT _regViewport;
	GFX3D_Viewport _currentViewport;
	POLYGON_ATTR _polyAttribute;
	PolygonPrimitiveType _vtxFormat;
	TEXIMAGE_PARAM _texParam;
	TextureTransformationMode _texCoordTransformMode;
	u32 _texPalette;
	u8 _vtxCoord16CurrentIndex;
	
	bool _inBegin;
	size_t _vtxCount; // the number of vertices registered in this list
	u16 _vtxIndex[4]; // indices to the main vert list
	bool _isGeneratingFirstPolyOfStrip;
	bool _generateTriangleStripIndexToggle;
	
	u8 _boxTestCoordCurrentIndex;
	u8 _positionTestCoordCurrentIndex;
	CACHE_ALIGN u16 _boxTestCoord16[6];
	CACHE_ALIGN Vector4s32 _positionTestVtx32;
	
	u32 _regLightColor[4];
	u32 _regLightDirection[4];
	u16 _regDiffuse;
	u16 _regAmbient;
	u16 _regSpecular;
	u16 _regEmission;
	u8 _shininessTablePendingIndex;
	
	CACHE_ALIGN Vector4s32 _vecLightDirectionTransformed[4];
	CACHE_ALIGN Vector4s32 _vecLightDirectionHalfNegative[4];
	bool _doesLightHalfVectorNeedUpdate[4];
	
	// This enum serves no real functional purpose except to be used for save state compatibility.
	enum LastMtxMultCommand
	{
		LastMtxMultCommand_4x4 = 0,
		LastMtxMultCommand_4x3 = 1,
		LastMtxMultCommand_3x3 = 2
	} _lastMtxMultCommand;
	
	void _UpdateTransformedTexCoordsIfNeeded();
	
public:
	NDSGeometryEngine();
	
	void Reset();
	
	MatrixMode GetMatrixMode() const;
	u32 GetLightDirectionRegisterAtIndex(const size_t i) const;
	u32 GetLightColorRegisterAtIndex(const size_t i) const;
	
	u8 GetMatrixStackIndex(const MatrixMode whichMatrix) const;
	void ResetMatrixStackPointer();
	
	void SetMatrixMode(const u32 param);
	bool SetCurrentMatrixLoad4x4(const u32 param);
	bool SetCurrentMatrixLoad4x3(const u32 param);
	bool SetCurrentMatrixMultiply4x4(const u32 param);
	bool SetCurrentMatrixMultiply4x3(const u32 param);
	bool SetCurrentMatrixMultiply3x3(const u32 param);
	bool SetCurrentScaleVector(const u32 param);
	bool SetCurrentTranslateVector(const u32 param);
	
	void MatrixPush();
	void MatrixPop(const u32 param);
	void MatrixStore(const u32 param);
	void MatrixRestore(const u32 param);
	void MatrixLoadIdentityToCurrent();
	void MatrixLoad4x4();
	void MatrixLoad4x3();
	void MatrixMultiply4x4();
	void MatrixMultiply4x3();
	void MatrixMultiply3x3();
	void MatrixScale();
	void MatrixTranslate();
	
	void SetDiffuseAmbient(const u32 param);
	void SetSpecularEmission(const u32 param);
	void SetLightDirection(const u32 param);
	void SetLightColor(const u32 param);
	void SetShininess(const u32 param);
	void SetNormal(const u32 param);
	
	void SetViewport(const u32 param);
	void SetViewport(const IOREG_VIEWPORT regViewport);
	void SetViewport(const GFX3D_Viewport viewport);
	void SetVertexColor(const u32 param);
	void SetVertexColor(const Color4u8 vtxColor555X);
	void SetTextureParameters(const u32 param);
	void SetTextureParameters(const TEXIMAGE_PARAM texParams);
	void SetTexturePalette(const u32 texPalette);
	void SetTextureCoordinates2s16(const u32 param);
	void SetTextureCoordinates2s16(const Vector2s16 &texCoord16);
	
	void VertexListBegin(const u32 param, const POLYGON_ATTR polyAttr);
	void VertexListBegin(const PolygonPrimitiveType vtxFormat, const POLYGON_ATTR polyAttr);
	void VertexListEnd();
	bool SetCurrentVertexPosition2s16(const u32 param);
	bool SetCurrentVertexPosition2s16(const Vector2s16 inVtxCoord16x2);
	void SetCurrentVertexPosition3s10(const u32 param);
	void SetCurrentVertexPosition(const Vector3s16 inVtxCoord16x3);
	template<size_t ONE, size_t TWO> void SetCurrentVertexPosition2s16Immediate(const u32 param);
	template<size_t ONE, size_t TWO> void SetCurrentVertexPosition2s16Immediate(const Vector2s16 inVtxCoord16x2);
	void SetCurrentVertexPosition3s10Relative(const u32 param);
	void SetCurrentVertexPositionRelative(const Vector3s16 inVtxCoord16x3);
	void AddCurrentVertexToList(GFX3D_GeometryList &targetGList);
	void GeneratePolygon(POLY &targetPoly, GFX3D_GeometryList &targetGList);
	
	bool SetCurrentBoxTestCoords(const u32 param);
	void BoxTest();
	bool SetCurrentPositionTestCoords(const u32 param);
	void PositionTest();
	void VectorTest(const u32 param);
	
	u32 GetClipMatrixAtIndex(const u32 requestedIndex) const;
	u32 GetDirectionalMatrixAtIndex(const u32 requestedIndex) const;
	u32 GetPositionTestResult(const u32 requestedIndex) const;
	
	void MatrixCopyFromCurrent(const MatrixMode whichMatrix, NDSMatrixFloat &outMtx);
	void MatrixCopyFromCurrent(const MatrixMode whichMatrix, NDSMatrix &outMtx);
	void MatrixCopyFromStack(const MatrixMode whichMatrix, const size_t stackIndex, NDSMatrixFloat &outMtx);
	void MatrixCopyFromStack(const MatrixMode whichMatrix, const size_t stackIndex, NDSMatrix &outMtx);
	void MatrixCopyToStack(const MatrixMode whichMatrix, const size_t stackIndex, const NDSMatrix &inMtx);
	void UpdateLightDirectionHalfAngleVector(const size_t index);
	void UpdateMatrixProjectionLua();
	
	void SaveState_LegacyFormat(GeometryEngineLegacySave &outLegacySave);
	void LoadState_LegacyFormat(const GeometryEngineLegacySave &inLegacySave);
	
	void SaveState_v2(EMUFILE &os);
	void LoadState_v2(EMUFILE &is);
	
	void SaveState_v4(EMUFILE &os);
	void LoadState_v4(EMUFILE &is);
};

//---------------------

extern CACHE_ALIGN u32 dsDepthExtend_15bit_to_24bit[32768];

void gfx3d_glFlush(const u32 v);
// end GE commands

void gfx3d_glFogColor(const u32 v);
void gfx3d_glFogOffset(const u32 v);
template<typename T> void gfx3d_glClearColor(const u8 offset, const T v);
template<typename T, size_t ADDROFFSET> void gfx3d_glClearDepth(const T v);
template<typename T, size_t ADDROFFSET> void gfx3d_glClearImageOffset(const T v);
void gfx3d_glSwapScreen(u32 screen);
u32 gfx3d_GetNumPolys();
u32 gfx3d_GetNumVertex();
template<typename T> void gfx3d_UpdateEdgeMarkColorTable(const u8 offset, const T v);
template<typename T> void gfx3d_UpdateFogTable(const u8 offset, const T v);
template<typename T> void gfx3d_UpdateToonTable(const u8 offset, const T v);
u32 gfx3d_GetClipMatrix(const u32 index);
u32 gfx3d_GetDirectionalMatrix(const u32 index);
void gfx3d_glAlphaFunc(u32 v);
u32 gfx3d_glGetPosRes(const u32 index);
u16 gfx3d_glGetVecRes(const u32 index);
void gfx3d_VBlankSignal();
void gfx3d_VBlankEndSignal(bool skipFrame);
void gfx3d_execute3D();
void gfx3d_sendCommandToFIFO(const u32 v);
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
const GFX3D_IOREG& GFX3D_GetIORegisterMap();
void ParseReg_DISP3DCNT();

u8 GFX3D_GetMatrixStackIndex(const MatrixMode whichMatrix);
void GFX3D_ResetMatrixStackPointer();

bool GFX3D_IsSwapBuffersPending();

void GFX3D_HandleGeometryPowerOff();

u32 GFX3D_GetRender3DFrameCount();
void GFX3D_ResetRender3DFrameCount();

template<ClipperMode CLIPPERMODE> PolygonType GFX3D_GenerateClippedPoly(const u16 rawPolyIndex, const PolygonType rawPolyType, const NDSVertex *(&rawVtx)[4], CPoly &outCPoly);

#endif //_GFX3D_H_
