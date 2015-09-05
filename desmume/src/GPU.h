/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 Theo Berkau
	Copyright (C) 2007 shash
	Copyright (C) 2009-2015 DeSmuME team

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

#ifndef GPU_H
#define GPU_H

#include <stdio.h>
#include <iosfwd>

#include "types.h"

#ifdef ENABLE_SSE2
#include <emmintrin.h>
#endif

class GPUEngineBase;
class EMUFILE;
struct MMU_struct;

//#undef FORCEINLINE
//#define FORCEINLINE

#define GPU_FRAMEBUFFER_NATIVE_WIDTH	256
#define GPU_FRAMEBUFFER_NATIVE_HEIGHT	192

#define GPU_VRAM_BLOCK_LINES			256

void gpu_savestate(EMUFILE* os);
bool gpu_loadstate(EMUFILE* is, int size);

typedef void (*rot_fun)(GPUEngineBase *gpu, const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *pal, const size_t i);

/*******************************************************************************
    this structure is for display control,
    it holds flags for general display
*******************************************************************************/

#ifdef LOCAL_BE
struct _DISPCNT
{
/* 7*/  u8 ForceBlank:1;      // A+B:
/* 6*/  u8 OBJ_BMP_mapping:1; // A+B: 0=2D (128KB), 1=1D (128..256KB)
/* 5*/  u8 OBJ_BMP_2D_dim:1;  // A+B: 0=128x512,    1=256x256 pixels
/* 4*/  u8 OBJ_Tile_mapping:1;// A+B: 0=2D (32KB),  1=1D (32..256KB)
/* 3*/  u8 BG0_3D:1;          // A  : 0=2D,         1=3D
/* 0*/  u8 BG_Mode:3;         // A+B:
/*15*/  u8 WinOBJ_Enable:1;   // A+B: 0=disable, 1=Enable
/*14*/  u8 Win1_Enable:1;     // A+B: 0=disable, 1=Enable
/*13*/  u8 Win0_Enable:1;     // A+B: 0=disable, 1=Enable
/*12*/  u8 OBJ_Enable:1;      // A+B: 0=disable, 1=Enable
/*11*/  u8 BG3_Enable:1;      // A+B: 0=disable, 1=Enable
/*10*/  u8 BG2_Enable:1;      // A+B: 0=disable, 1=Enable
/* 9*/  u8 BG1_Enable:1;      // A+B: 0=disable, 1=Enable
/* 8*/  u8 BG0_Enable:1;        // A+B: 0=disable, 1=Enable
/*23*/  u8 OBJ_HBlank_process:1;    // A+B: OBJ processed during HBlank (GBA bit5)
/*22*/  u8 OBJ_BMP_1D_Bound:1;      // A  :
/*20*/  u8 OBJ_Tile_1D_Bound:2;     // A+B:
/*18*/  u8 VRAM_Block:2;            // A  : VRAM block (0..3=A..D)

/*16*/  u8 DisplayMode:2;     // A+B: coreA(0..3) coreB(0..1) GBA(Green Swap)
                                    // 0=off (white screen)
                                    // 1=on (normal BG & OBJ layers)
                                    // 2=VRAM display (coreA only)
                                    // 3=RAM display (coreA only, DMA transfers)

/*31*/  u8 ExOBJPalette_Enable:1;   // A+B: 0=disable, 1=Enable OBJ extended Palette
/*30*/  u8 ExBGxPalette_Enable:1;   // A+B: 0=disable, 1=Enable BG extended Palette
/*27*/  u8 ScreenBase_Block:3;      // A  : Screen Base (64K step)
/*24*/  u8 CharacBase_Block:3;      // A  : Character Base (64K step)
};
#else
struct _DISPCNT
{
/* 0*/  u8 BG_Mode:3;         // A+B:
/* 3*/  u8 BG0_3D:1;          // A  : 0=2D,         1=3D
/* 4*/  u8 OBJ_Tile_mapping:1;     // A+B: 0=2D (32KB),  1=1D (32..256KB)
/* 5*/  u8 OBJ_BMP_2D_dim:1;  // A+B: 0=128x512,    1=256x256 pixels
/* 6*/  u8 OBJ_BMP_mapping:1; // A+B: 0=2D (128KB), 1=1D (128..256KB)

                                    // 7-15 same as GBA
/* 7*/  u8 ForceBlank:1;      // A+B:
/* 8*/  u8 BG0_Enable:1;        // A+B: 0=disable, 1=Enable
/* 9*/  u8 BG1_Enable:1;      // A+B: 0=disable, 1=Enable
/*10*/  u8 BG2_Enable:1;      // A+B: 0=disable, 1=Enable
/*11*/  u8 BG3_Enable:1;      // A+B: 0=disable, 1=Enable
/*12*/  u8 OBJ_Enable:1;      // A+B: 0=disable, 1=Enable
/*13*/  u8 Win0_Enable:1;     // A+B: 0=disable, 1=Enable
/*14*/  u8 Win1_Enable:1;     // A+B: 0=disable, 1=Enable
/*15*/  u8 WinOBJ_Enable:1;   // A+B: 0=disable, 1=Enable

/*16*/  u8 DisplayMode:2;     // A+B: coreA(0..3) coreB(0..1) GBA(Green Swap)
                                    // 0=off (white screen)
                                    // 1=on (normal BG & OBJ layers)
                                    // 2=VRAM display (coreA only)
                                    // 3=RAM display (coreA only, DMA transfers)

/*18*/  u8 VRAM_Block:2;            // A  : VRAM block (0..3=A..D)
/*20*/  u8 OBJ_Tile_1D_Bound:2;     // A+B:
/*22*/  u8 OBJ_BMP_1D_Bound:1;      // A  :
/*23*/  u8 OBJ_HBlank_process:1;    // A+B: OBJ processed during HBlank (GBA bit5)
/*24*/  u8 CharacBase_Block:3;      // A  : Character Base (64K step)
/*27*/  u8 ScreenBase_Block:3;      // A  : Screen Base (64K step)
/*30*/  u8 ExBGxPalette_Enable:1;   // A+B: 0=disable, 1=Enable BG extended Palette
/*31*/  u8 ExOBJPalette_Enable:1;   // A+B: 0=disable, 1=Enable OBJ extended Palette
};
#endif

union FragmentColor
{
	u32 color;
	struct
	{
		u8 r,g,b,a;
	};
};

typedef union
{
    struct _DISPCNT bits;
    u32 val;
} DISPCNT;
#define BGxENABLED(cnt,num)    ((num<8)? ((cnt.val>>8) & num):0)


enum BlendFunc
{
	NoBlend, Blend, Increase, Decrease
};


/*******************************************************************************
    this structure is for display control of a specific layer,
    there are 4 background layers
    their priority indicate which one to draw on top of the other
    some flags indicate special drawing mode, size, FX
*******************************************************************************/

#ifdef LOCAL_BE
struct _BGxCNT
{
/* 7*/ u8 Palette_256:1;         // 0=16x16, 1=1*256 palette
/* 6*/ u8 Mosaic_Enable:1;       // 0=disable, 1=Enable mosaic
/* 2*/ u8 CharacBase_Block:4;    // individual character base offset (n*16KB)
/* 0*/ u8 Priority:2;            // 0..3=high..low
/*14*/ u8 ScreenSize:2;          // text    : 256x256 512x256 256x512 512x512
                                       // x/rot/s : 128x128 256x256 512x512 1024x1024
                                       // bmp     : 128x128 256x256 512x256 512x512
                                       // large   : 512x1024 1024x512 - -
/*13*/ u8 PaletteSet_Wrap:1;     // BG0 extended palette set 0=set0, 1=set2
                                       // BG1 extended palette set 0=set1, 1=set3
                                       // BG2 overflow area wraparound 0=off, 1=wrap
                                       // BG3 overflow area wraparound 0=off, 1=wrap
/* 8*/ u8 ScreenBase_Block:5;    // individual screen base offset (text n*2KB, BMP n*16KB)
};
#else
struct _BGxCNT
{
/* 0*/ u8 Priority:2;            // 0..3=high..low
/* 2*/ u8 CharacBase_Block:4;    // individual character base offset (n*16KB)
/* 6*/ u8 Mosaic_Enable:1;       // 0=disable, 1=Enable mosaic
/* 7*/ u8 Palette_256:1;         // 0=16x16, 1=1*256 palette
/* 8*/ u8 ScreenBase_Block:5;    // individual screen base offset (text n*2KB, BMP n*16KB)
/*13*/ u8 PaletteSet_Wrap:1;     // BG0 extended palette set 0=set0, 1=set2
                                       // BG1 extended palette set 0=set1, 1=set3
                                       // BG2 overflow area wraparound 0=off, 1=wrap
                                       // BG3 overflow area wraparound 0=off, 1=wrap
/*14*/ u8 ScreenSize:2;          // text    : 256x256 512x256 256x512 512x512
                                       // x/rot/s : 128x128 256x256 512x512 1024x1024
                                       // bmp     : 128x128 256x256 512x256 512x512
                                       // large   : 512x1024 1024x512 - -
};
#endif


typedef union
{
    struct _BGxCNT bits;
    u16 val;
} BGxCNT;

/*******************************************************************************
    this structure is for background offset
*******************************************************************************/

typedef struct {
    u16 BGxHOFS;
    u16 BGxVOFS;
} BGxOFS;

/*******************************************************************************
    this structure is for rotoscale parameters
*******************************************************************************/

typedef struct {
    s16 BGxPA;
    s16 BGxPB;
    s16 BGxPC;
    s16 BGxPD;
    s32 BGxX;
    s32 BGxY;
} BGxPARMS;


/*******************************************************************************
	these structures are for window description,
	windows are square regions and can "subclass"
	background layers or object layers (i.e window controls the layers)

	screen
	|
	+-- Window0/Window1/OBJwindow/OutOfWindows
		|
		+-- BG0/BG1/BG2/BG3/OBJ
*******************************************************************************/

typedef union {
	struct	{
		u8 end:8;
		u8 start:8;
	} bits ;
	u16 val;
} WINxDIM;

#ifdef LOCAL_BE
typedef struct {
/* 6*/  u8 :2;
/* 5*/  u8 WINx_Effect_Enable:1;
/* 4*/  u8 WINx_OBJ_Enable:1;
/* 3*/  u8 WINx_BG3_Enable:1;
/* 2*/  u8 WINx_BG2_Enable:1;
/* 1*/  u8 WINx_BG1_Enable:1;
/* 0*/  u8 WINx_BG0_Enable:1;
} WINxBIT;
#else
typedef struct {
/* 0*/  u8 WINx_BG0_Enable:1;
/* 1*/  u8 WINx_BG1_Enable:1;
/* 2*/  u8 WINx_BG2_Enable:1;
/* 3*/  u8 WINx_BG3_Enable:1;
/* 4*/  u8 WINx_OBJ_Enable:1;
/* 5*/  u8 WINx_Effect_Enable:1;
/* 6*/  u8 :2;
} WINxBIT;
#endif

#ifdef LOCAL_BE
typedef union {
	struct {
		WINxBIT win0;
		WINxBIT win1;
	} bits;
	struct {
		u8 :3;
		u8 win0_en:5;
		u8 :3;
		u8 win1_en:5;
	} packed_bits;
	struct {
		u8 low;
		u8 high;
	} bytes;
	u16 val ;
} WINxCNT ;
#else
typedef union {
	struct {
		WINxBIT win0;
		WINxBIT win1;
	} bits;
	struct {
		u8 win0_en:5;
		u8 :3;
		u8 win1_en:5;
		u8 :3;
	} packed_bits;
	struct {
		u8 low;
		u8 high;
	} bytes;
	u16 val ;
} WINxCNT ;
#endif

/*
typedef struct {
    WINxDIM WIN0H;
    WINxDIM WIN1H;
    WINxDIM WIN0V;
    WINxDIM WIN1V;
    WINxCNT WININ;
    WINxCNT WINOUT;
} WINCNT;
*/

/*******************************************************************************
    this structure is for miscellanous settings
    //TODO: needs further description
*******************************************************************************/

typedef struct {
    u16 MOSAIC;
    u16 unused1;
    u16 unused2;//BLDCNT;
    u16 unused3;//BLDALPHA;
    u16 unused4;//BLDY;
    u16 unused5;
	/*
    u16 unused6;
    u16 unused7;
    u16 unused8;
    u16 unused9;
	*/
} MISCCNT;


/*******************************************************************************
    this structure is for 3D settings
*******************************************************************************/

struct _DISP3DCNT
{
/* 0*/ u8 EnableTexMapping:1;    //
/* 1*/ u8 PolygonShading:1;      // 0=Toon Shading, 1=Highlight Shading
/* 2*/ u8 EnableAlphaTest:1;     // see ALPHA_TEST_REF
/* 3*/ u8 EnableAlphaBlending:1; // see various Alpha values
/* 4*/ u8 EnableAntiAliasing:1;  //
/* 5*/ u8 EnableEdgeMarking:1;   // see EDGE_COLOR
/* 6*/ u8 FogOnlyAlpha:1;        // 0=Alpha and Color, 1=Only Alpha (see FOG_COLOR)
/* 7*/ u8 EnableFog:1;           // Fog Master Enable
/* 8*/ u8 FogShiftSHR:4;         // 0..10 SHR-Divider (see FOG_OFFSET)
/*12*/ u8 AckColorBufferUnderflow:1; // Color Buffer RDLINES Underflow (0=None, 1=Underflow/Acknowledge)
/*13*/ u8 AckVertexRAMOverflow:1;    // Polygon/Vertex RAM Overflow    (0=None, 1=Overflow/Acknowledge)
/*14*/ u8 RearPlaneMode:1;       // 0=Blank, 1=Bitmap
/*15*/ u8 :1;
/*16*/ u16 :16;
};

typedef union
{
    struct _DISP3DCNT bits;
    u32 val;
} DISP3DCNT;

/*******************************************************************************
    this structure is for capture control (core A only)

    source:
    http://nocash.emubase.de/gbatek.htm#dsvideocaptureandmainmemorydisplaymode
*******************************************************************************/
struct DISPCAPCNT
{
	enum CAPX {
		_128, _256
	} capx;
    u32 val;
	BOOL enabled;
	u8 EVA;
	u8 EVB;
	u8 writeBlock;
	u8 writeOffset;
	u16 capy;
	u8 srcA;
	u8 srcB;
	u8 readBlock;
	u8 readOffset;
	u8 capSrc;
} ;

/*******************************************************************************
    this structure holds everything and should be mapped to
    * core A : 0x04000000
    * core B : 0x04001000
*******************************************************************************/

typedef struct _reg_dispx {
    DISPCNT dispx_DISPCNT;            // 0x0400x000
    u16 dispA_DISPSTAT;               // 0x04000004
    u16 dispx_VCOUNT;                 // 0x0400x006
    BGxCNT dispx_BGxCNT[4];           // 0x0400x008
    BGxOFS dispx_BGxOFS[4];           // 0x0400x010
    BGxPARMS dispx_BG2PARMS;          // 0x0400x020
    BGxPARMS dispx_BG3PARMS;          // 0x0400x030
    u8			filler[12];           // 0x0400x040
    MISCCNT dispx_MISC;               // 0x0400x04C
    DISP3DCNT dispA_DISP3DCNT;        // 0x04000060
    DISPCAPCNT dispA_DISPCAPCNT;      // 0x04000064
    u32 dispA_DISPMMEMFIFO;           // 0x04000068
} REG_DISPx ;

enum GPUEngineID
{
	GPUEngineID_Main	= 0,
	GPUEngineID_Sub		= 1
};

/* human readable bitmask names */
#define ADDRESS_STEP_512B	   0x00200
#define ADDRESS_STEP_1KB		0x00400
#define ADDRESS_STEP_2KB		0x00800
#define ADDRESS_STEP_4KB		0x01000
#define ADDRESS_STEP_8KB		0x02000
#define ADDRESS_STEP_16KB	   0x04000
#define ADDRESS_STEP_32KB	   0x08000
#define ADDRESS_STEP_64KB	   0x10000
#define ADDRESS_STEP_128KB	   0x20000
#define ADDRESS_STEP_256KB	   0x40000
#define ADDRESS_STEP_512KB	   0x80000
#define ADDRESS_MASK_256KB	   (ADDRESS_STEP_256KB-1)

#ifdef LOCAL_BE
struct _TILEENTRY
{
/*14*/	unsigned Palette:4;
/*13*/	unsigned VFlip:1;	// VERTICAL FLIP (top<-->bottom)
/*12*/	unsigned HFlip:1;	// HORIZONTAL FLIP (left<-->right)
/* 0*/	unsigned TileNum:10;
};
#else
struct _TILEENTRY
{
/* 0*/	unsigned TileNum:10;
/*12*/	unsigned HFlip:1;	// HORIZONTAL FLIP (left<-->right)
/*13*/	unsigned VFlip:1;	// VERTICAL FLIP (top<-->bottom)
/*14*/	unsigned Palette:4;
};
#endif
typedef union
{
	struct _TILEENTRY bits;
	u16 val;
} TILEENTRY;

struct _ROTOCOORD
{
	u32 Fraction:8;
	s32 Integer:20;
	u32 pad:4;
};
typedef union
{
	struct _ROTOCOORD bits;
	s32 val;
} ROTOCOORD;


/*
	this structure is for color representation,
	it holds 5 meaningful bits per color channel (red,green,blue)
	and	  1 meaningful bit for alpha representation
	this bit can be unused or used for special FX
*/

struct _COLOR { // abgr x555
#ifdef LOCAL_BE
	unsigned alpha:1;    // sometimes it is unused (pad)
	unsigned blue:5;
	unsigned green:5;
	unsigned red:5;
#else
     unsigned red:5;
     unsigned green:5;
     unsigned blue:5;
     unsigned alpha:1;    // sometimes it is unused (pad)
#endif
};
struct _COLORx { // abgr x555
	unsigned bgr:15;
	unsigned alpha:1;	// sometimes it is unused (pad)
};

typedef union
{
	struct _COLOR bits;
	struct _COLORx bitx;
	u16 val;
} COLOR;

struct _COLOR32 { // ARGB
	unsigned :3;
	unsigned blue:5;
	unsigned :3;
	unsigned green:5;
	unsigned :3;
	unsigned red:5;
	unsigned :7;
	unsigned alpha:1;	// sometimes it is unused (pad)
};

typedef union
{
	struct _COLOR32 bits;
	u32 val;
} COLOR32;

#define COLOR_16_32(w,i)	\
	/* doesnt matter who's 16bit who's 32bit */ \
	i.bits.red   = w.bits.red; \
	i.bits.green = w.bits.green; \
	i.bits.blue  = w.bits.blue; \
	i.bits.alpha = w.bits.alpha;



 // (00: Normal, 01: Transparent, 10: Object window, 11: Bitmap)
enum GPU_OBJ_MODE
{
	GPU_OBJ_MODE_Normal = 0,
	GPU_OBJ_MODE_Transparent = 1,
	GPU_OBJ_MODE_Window = 2,
	GPU_OBJ_MODE_Bitmap = 3
};

typedef union
{
	u16 attr[4];
	
	struct
	{
#ifdef LOCAL_BE
		//attr0
		unsigned Shape:2;
		unsigned Depth:1;
		unsigned Mosaic:1;
		unsigned Mode:2;
		unsigned RotScale:2;
		unsigned Y:8;
		
		//attr1
		unsigned Size:2;
		unsigned VFlip:1;
		unsigned HFlip:1;
		unsigned RotScalIndex:3;
		signed X:9;
		
		//attr2
		unsigned PaletteIndex:4;
		unsigned Priority:2;
		unsigned TileIndex:10;
		
		//attr3
		unsigned attr3:16; // Whenever this is used, you will need to explicitly convert endianness.
#else
		//attr0
		unsigned Y:8;
		unsigned RotScale:2;
		unsigned Mode:2;
		unsigned Mosaic:1;
		unsigned Depth:1;
		unsigned Shape:2;
		
		//attr1
		signed X:9;
		unsigned RotScalIndex:3;
		unsigned HFlip:1;
		unsigned VFlip:1;
		unsigned Size:2;
		
		//attr2
		unsigned TileIndex:10;
		unsigned Priority:2;
		unsigned PaletteIndex:4;
		
		//attr3
		unsigned attr3:16; // Whenever this is used, you will need to explicitly convert endianness.
#endif
	};
	
} OAMAttributes;

enum SpriteRenderMode
{
	SpriteRenderMode_Sprite1D = 0,
	SpriteRenderMode_Sprite2D = 1
};

typedef struct
{
	 s16 x;
	 s16 y;
} SpriteSize;

typedef u8 TBlendTable[32][32];

#define NB_PRIORITIES	4
#define NB_BG		4
//this structure holds information for rendering.
typedef struct
{
	u8 PixelsX[256];
	u8 BGs[NB_BG], nbBGs;
	u8 pad[1];
	u16 nbPixelsX;
	//256+8:
	u8 pad2[248];

	//things were slower when i organized this struct this way. whatever.
	//u8 PixelsX[256];
	//int BGs[NB_BG], nbBGs;
	//int nbPixelsX;
	////<-- 256 + 24
	//u8 pad2[256-24];
} itemsForPriority_t;
#define MMU_ABG		0x06000000
#define MMU_BBG		0x06200000
#define MMU_AOBJ	0x06400000
#define MMU_BOBJ	0x06600000
#define MMU_LCDC	0x06800000

enum GPULayerID
{
	GPULayerID_BG0					= 0,
	GPULayerID_BG1					= 1,
	GPULayerID_BG2					= 2,
	GPULayerID_BG3					= 3,
	GPULayerID_OBJ					= 4,
	
	GPULayerID_None					= 5
};

enum BGType
{
	BGType_Invalid					= 0,
	BGType_Text						= 1,
	BGType_Affine					= 2,
	BGType_Large8bpp				= 3,
	
	BGType_AffineExt				= 4,
	BGType_AffineExt_256x16			= 5,
	BGType_AffineExt_256x1			= 6,
	BGType_AffineExt_Direct			= 7
};

enum GPUDisplayMode
{
	GPUDisplayMode_Off				= 0,
	GPUDisplayMode_Normal			= 1,
	GPUDisplayMode_VRAM				= 2,
	GPUDisplayMode_MainMemory		= 3
};

enum GPUMasterBrightMode
{
	GPUMasterBrightMode_Disable		= 0,
	GPUMasterBrightMode_Up			= 1,
	GPUMasterBrightMode_Down		= 2,
	GPUMasterBrightMode_Reserved	= 3
	
};

enum GPULayerType
{
	GPULayerType_3D					= 0,
	GPULayerType_BG					= 1,
	GPULayerType_OBJ				= 2
};

enum NDSDisplayID
{
	NDSDisplayID_Main				= 0,
	NDSDisplayID_Touch				= 1
};

typedef struct
{
	u8 blockIndexDisplayVRAM;
	bool isBlockUsed[4];
} VRAM3DUsageProperties;

typedef struct
{
	bool isCustomSizeRequested;			// true - The user requested a custom size; false - The user requested the native size
	size_t customWidth;					// The custom buffer width, measured in pixels
	size_t customHeight;				// The custom buffer height, measured in pixels
	u16 *masterCustomBuffer;			// Pointer to the head of the master custom buffer.
	u16 *masterNativeBuffer;			// Pointer to the head of the master native buffer.
	
	u16 *customBuffer[2];				// Pointer to a display's custom size framebuffer
	u16	*nativeBuffer[2];				// Pointer to a display's native size framebuffer
	
	bool didPerformCustomRender[2];		// true - The display performed a custom render; false - The display performed a native render
	size_t renderedWidth[2];			// The display rendered at this width, measured in pixels
	size_t renderedHeight[2];			// The display rendered at this height, measured in pixels
	u16 *renderedBuffer[2];				// The display rendered to this buffer
} NDSDisplayInfo;

#define VRAM_NO_3D_USAGE 0xFF

class GPUEngineBase
{
protected:
	static CACHE_ALIGN u16 _fadeInColors[17][0x8000];
	static CACHE_ALIGN u16 _fadeOutColors[17][0x8000];
	static CACHE_ALIGN u8 _blendTable555[17][17][32][32];
	
	static const CACHE_ALIGN SpriteSize _sprSizeTab[4][4];
	static const CACHE_ALIGN BGType _mode2type[8][4];
	static const CACHE_ALIGN u16 _sizeTab[8][4][2];
	static const CACHE_ALIGN u8 _winEmpty[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	
	static struct MosaicLookup {
		
		struct TableEntry {
			u8 begin, trunc;
		} table[16][256];
		
		MosaicLookup() {
			for(int m=0;m<16;m++)
				for(int i=0;i<256;i++) {
					int mosaic = m+1;
					TableEntry &te = table[m][i];
					te.begin = (i%mosaic==0);
					te.trunc = i/mosaic*mosaic;
				}
		}
		
		TableEntry *width, *height;
		int widthValue, heightValue;
		
	} _mosaicLookup;
	
	CACHE_ALIGN u16 _sprColor[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _sprAlpha[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _sprType[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _sprPrio[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _sprWin[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	
	bool _enableLayer[5];
	itemsForPriority_t _itemsForPriority[NB_PRIORITIES];
	
	struct MosaicColor {
		u16 bg[4][256];
		struct Obj {
			u16 color;
			u8 alpha;
			u8 opaque;
		} obj[256];
	} _mosaicColors;
	
	GPUEngineID _engineID;
	u16 *_paletteBG;
	u16 *_paletteOBJ;
	OAMAttributes *_oamList;
	u32 _sprMem;
	
	u8 _bgPrio[5];
	bool _bg0HasHighestPrio;
	
	u8 _sprBoundary;
	u8 _sprBMPBoundary;
	u8 _sprBMPMode;
	u32 _sprEnable;
	
	u16 *_currentFadeInColors;
	u16 *_currentFadeOutColors;
	
	bool _blend1;
	bool _blend2[8];
	
	TBlendTable *_blendTable;
	
	u32 _BG_bmp_large_ram[4];
	u32 _BG_bmp_ram[4];
	u32 _BG_tile_ram[4];
	u32 _BG_map_ram[4];
	
	BGType _BGTypes[4];
	
	GPUDisplayMode _dispMode;
	u8 _vramBlock;
	
	CACHE_ALIGN u8 _sprNum[256];
	CACHE_ALIGN u8 _h_win[2][GPU_FRAMEBUFFER_NATIVE_WIDTH];
	const u8 *_curr_win[2];
	
	NDSDisplayID _targetDisplayID;
	u16 *_VRAMaddrNative;
	u16 *_VRAMaddrCustom;
	
	bool _curr_mosaic_enabled;
	
	int _finalColorBckFuncID;
	int _finalColor3DFuncID;
	int _finalColorSpriteFuncID;
	
	SpriteRenderMode _spriteRenderMode;
	
	u8 *_bgPixels;
	
	u8 _WIN0H0;
	u8 _WIN0H1;
	u8 _WIN0V0;
	u8 _WIN0V1;
	
	u8 _WIN1H0;
	u8 _WIN1H1;
	u8 _WIN1V0;
	u8 _WIN1V1;
	
	u8 _WININ0;
	bool _WININ0_SPECIAL;
	u8 _WININ1;
	bool _WININ1_SPECIAL;
	
	u8 _WINOUT;
	bool _WINOUT_SPECIAL;
	u8 _WINOBJ;
	bool _WINOBJ_SPECIAL;
	
	u8 _WIN0_ENABLED;
	u8 _WIN1_ENABLED;
	u8 _WINOBJ_ENABLED;
	
	u16 _BLDCNT;
	u8 _BLDALPHA_EVA;
	u8 _BLDALPHA_EVB;
	u8 _BLDY_EVY;
	
	void _InitLUTs();
	void _Reset_Base();
	
	void _MosaicSpriteLinePixel(const size_t x, u16 l, u16 *dst, u8 *dst_alpha, u8 *typeTab, u8 *prioTab);
	void _MosaicSpriteLine(u16 l, u16 *dst, u8 *dst_alpha, u8 *typeTab, u8 *prioTab);
	
	template<rot_fun fun, bool WRAP> void _rot_scale_op(const BGxPARMS &param, const u16 LG, const s32 wh, const s32 ht, const u32 map, const u32 tile, const u16 *pal);
	template<GPULayerID LAYERID, rot_fun fun> void _apply_rot_fun(const BGxPARMS &param, const u16 LG, const u32 map, const u32 tile, const u16 *pal);
	
	template<GPULayerID LAYERID, bool MOSAIC, bool ISCUSTOMRENDERINGNEEDED> void _LineLarge8bpp();
	template<GPULayerID LAYERID, bool MOSAIC, bool ISCUSTOMRENDERINGNEEDED> void _RenderLine_TextBG(u16 XBG, u16 YBG, u16 LG);
	
	template<GPULayerID LAYERID, bool MOSAIC, bool ISCUSTOMRENDERINGNEEDED> void _RotBG2(const BGxPARMS &param, const u16 LG);
	template<GPULayerID LAYERID, bool MOSAIC, bool ISCUSTOMRENDERINGNEEDED> void _ExtRotBG2(const BGxPARMS &param, const u16 LG);
	
	template<GPULayerID LAYERID, bool MOSAIC, bool ISCUSTOMRENDERINGNEEDED> void _LineText();
	template<GPULayerID LAYERID, bool MOSAIC, bool ISCUSTOMRENDERINGNEEDED> void _LineRot();
	template<GPULayerID LAYERID, bool MOSAIC, bool ISCUSTOMRENDERINGNEEDED> void _LineExtRot();
	
	template<int WIN_NUM> u8 _WithinRect(const size_t x) const;
	template <GPULayerID LAYERID> void _RenderLine_CheckWindows(const size_t srcX, bool &draw, bool &effect) const;
	
	template<bool ISCUSTOMRENDERINGNEEDED> void _RenderLine_Layer(const u16 l, u16 *dstLine, const size_t dstLineWidth, const size_t dstLineCount);
	template<bool ISCUSTOMRENDERINGNEEDED> void _RenderLine_MasterBrightness(u16 *dstLine, const size_t dstLineWidth, const size_t dstLineCount);
	
	template<size_t WIN_NUM> void _UpdateWINH();
	template<size_t WIN_NUM> void _SetupWindows();
	template<GPULayerID LAYERID, bool MOSAIC, bool ISCUSTOMRENDERINGNEEDED> void _ModeRender();
	
	template<GPULayerID LAYERID, bool BACKDROP, BlendFunc FUNC, bool WINDOW>
	FORCEINLINE FASTCALL bool _master_setFinalBGColor(const size_t srcX, const size_t dstX, const u16 *dstLine, const u8 *bgPixelsLine, u16 &outColor);
	
	template<GPULayerID LAYERID, BlendFunc FUNC, bool WINDOW>
	FORCEINLINE FASTCALL void _master_setFinal3dColor(const size_t srcX, const size_t dstX, u16 *dstLine, u8 *bgPixelsLine, const FragmentColor src);
	
	template<GPULayerID LAYERID, BlendFunc FUNC, bool WINDOW>
	FORCEINLINE FASTCALL void _master_setFinalOBJColor(const size_t srcX, const size_t dstX, u16 *dstLine, u8 *bgPixelsLine, const u16 src, const u8 alpha, const u8 type);
	
	template<GPULayerID LAYERID, bool BACKDROP, int FUNCNUM> void _SetFinalColorBG(const size_t srcX, const size_t dstX, u16 *dstLine, u8 *bgPixelsLine, u16 src);
	template<GPULayerID LAYERID> void _SetFinalColor3D(const size_t srcX, const size_t dstX, u16 *dstLine, u8 *bgPixelsLine, const FragmentColor src);
	template<GPULayerID LAYERID> void _SetFinalColorSprite(const size_t srcX, const size_t dstX, u16 *dstLine, u8 *bgPixelsLine, const u16 src, const u8 alpha, const u8 type);
	
	u16 _FinalColorBlend(const u16 colA, const u16 colB);
	FORCEINLINE u16 _FinalColorBlendFunc(const u16 colA, const u16 colB, const TBlendTable *blendTable);
	
	void _RenderSpriteBMP(const u8 spriteNum, const u16 l, u16 *dst, const u32 srcadr, u8 *dst_alpha, u8 *typeTab, u8 *prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha);
	void _RenderSprite256(const u8 spriteNum, const u16 l, u16 *dst, const u32 srcadr, const u16 *pal, u8 *dst_alpha, u8 *typeTab, u8 *prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha);
	void _RenderSprite16(const u16 l, u16 *dst, const u32 srcadr, const u16 *pal, u8 *dst_alpha, u8 *typeTab, u8 *prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha);
	void _RenderSpriteWin(const u8 *src, const bool col256, const size_t lg, size_t sprX, size_t x, const s32 xdir);
	bool _ComputeSpriteVars(const OAMAttributes &spriteInfo, const u16 l, SpriteSize &sprSize, s32 &sprX, s32 &sprY, s32 &x, s32 &y, s32 &lg, s32 &xdir);
	
	u32 _SpriteAddressBMP(const OAMAttributes &spriteInfo, const SpriteSize sprSize, const s32 y);
	
	template<SpriteRenderMode MODE> void _SpriteRenderPerform(u16 *dst, u8 *dst_alpha, u8 *typeTab, u8 *prioTab);
	
public:
	GPUEngineBase();
	virtual ~GPUEngineBase();
	
	virtual void Reset();
	void ResortBGLayers();
	void SetMasterBrightness(const u16 val);
	void SetupFinalPixelBlitter();
	
	void SetVideoProp(const u32 ctrlBits);
	template<GPULayerID LAYERID> void SetBGProp(const u16 ctrlBits);
	
	template<bool ISCUSTOMRENDERINGNEEDED> void RenderLine(const u16 l, bool skip);
	
	// some structs are becoming redundant
	// some functions too (no need to recopy some vars as it is done by MMU)
	REG_DISPx *dispx_st;

	//this indicates whether this gpu is handling debug tools
	bool debug;
	
	_BGxCNT & bgcnt(int num) { return (dispx_st)->dispx_BGxCNT[num].bits; }
	const _DISPCNT& dispCnt() const { return dispx_st->dispx_DISPCNT.bits; }
	
	u16 BGSize[4][2];
	u8 BGExtPalSlot[4];
	
	bool isCustomRenderingNeeded;
	bool is3DEnabled;
	u8 vramBGLayer;
	u8 vramBlockBGIndex;
	u8 vramBlockOBJIndex;
	
	u16 *customBuffer;
	u16 *nativeBuffer;
	size_t renderedWidth;
	size_t renderedHeight;
	u16 *renderedBuffer;
	
	u16 *workingScanline;
	GPUMasterBrightMode MasterBrightMode;
	u32 MasterBrightFactor;
	
	u32 currLine;
	u16 *currDst;
	
	bool need_update_winh[2];
	
	struct AffineInfo {
		AffineInfo() : x(0), y(0) {}
		u32 x, y;
	} affineInfo[2];
	
	bool GetEnableState();
	void SetEnableState(bool theState);
	bool GetLayerEnableState(const size_t layerIndex);
	void SetLayerEnableState(const size_t layerIndex, bool theState);
	
	template<GPULayerID LAYERID, bool BACKDROP, int FUNCNUM, bool ISCUSTOMRENDERINGNEEDED, bool USECUSTOMVRAM> FORCEINLINE void ____setFinalColorBck(const u16 color, const size_t srcX);
	template<GPULayerID LAYERID, bool MOSAIC, bool BACKDROP, int FUNCNUM, bool ISCUSTOMRENDERINGNEEDED, bool USECUSTOMVRAM> FORCEINLINE void ___setFinalColorBck(u16 color, const size_t srcX, const bool opaque);
	template<GPULayerID LAYERID, bool MOSAIC, bool BACKDROP, bool ISCUSTOMRENDERINGNEEDED> FORCEINLINE void __setFinalColorBck(u16 color, const size_t srcX, const bool opaque);
	
	void UpdateVRAM3DUsageProperties_BGLayer(const size_t bankIndex, VRAM3DUsageProperties &outProperty);
	void UpdateVRAM3DUsageProperties_OBJLayer(const size_t bankIndex, VRAM3DUsageProperties &outProperty);
	
	template<GPULayerID LAYERID, int SET_XY> void setAffineStart(u32 val);
	template<GPULayerID LAYERID, int SET_XY, bool HIWORD> void setAffineStartWord(u16 val);
	template<GPULayerID LAYERID, int SET_XY> u32 getAffineStart();
	template<GPULayerID LAYERID, int SET_XY> void refreshAffineStartRegs();
	
	void SpriteRender(u16 *dst, u8 *dst_alpha, u8 *typeTab, u8 *prioTab);
	void ModeRenderDebug(const GPULayerID layerID);

	template<bool ISCUSTOMRENDERINGNEEDED> void HandleDisplayModeOff(u16 *dstLine, const size_t l, const size_t dstLineWidth, const size_t dstLineCount);
	template<bool ISCUSTOMRENDERINGNEEDED> void HandleDisplayModeNormal(u16 *dstLine, const size_t l, const size_t dstLineWidth, const size_t dstLineCount);
	template<bool ISCUSTOMRENDERINGNEEDED> void HandleDisplayModeVRAM(u16 *dstLine, const size_t l, const size_t dstLineWidth, const size_t dstLineCount);
	template<bool ISCUSTOMRENDERINGNEEDED> void HandleDisplayModeMainMemory(u16 *dstLine, const size_t l, const size_t dstLineWidth, const size_t dstLineCount);
	
	u32 GetHOFS(const size_t bg) const;
	u32 GetVOFS(const size_t bg) const;

	void UpdateBLDALPHA();
	void SetBLDALPHA(const u16 val);
	void SetBLDALPHA_EVA(const u8 val);
	void SetBLDALPHA_EVB(const u8 val);
	
	// Blending
	void SetBLDCNT_LOW(const u8 val);
	void SetBLDCNT_HIGH(const u8 val);
	void SetBLDCNT(const u16 val);
	void SetBLDY_EVY(const u8 val);
	
	void SetWIN0_H(const u16 val);
	void SetWIN0_H0(const u8 val);
	void SetWIN0_H1(const u8 val);
	
	void SetWIN0_V(const u16 val);
	void SetWIN0_V0(const u8 val);
	void SetWIN0_V1(const u8 val);
	
	void SetWIN1_H(const u16 val);
	void SetWIN1_H0(const u8 val);
	void SetWIN1_H1(const u8 val);
	
	void SetWIN1_V(const u16 val);
	void SetWIN1_V0(const u8 val);
	void SetWIN1_V1(const u8 val);
	
	void SetWININ(const u16 val);
	void SetWININ0(const u8 val);
	void SetWININ1(const u8 val);
	
	void SetWINOUT16(const u16 val);
	void SetWINOUT(const u8 val);
	void SetWINOBJ(const u8 val);
	
	int GetFinalColorBckFuncID() const;
	void SetFinalColorBckFuncID(int funcID);

	NDSDisplayID GetDisplayByID() const;
	void SetDisplayByID(const NDSDisplayID theDisplayID);
	
	GPUEngineID GetEngineID() const;
	
	virtual void SetCustomFramebufferSize(size_t w, size_t h);
	void BlitNativeToCustomFramebuffer();
	
	void REG_DISPx_pack_test();
};

class GPUEngineA : public GPUEngineBase
{
protected:
	FragmentColor *_3DFramebufferRGBA6665;
	u16 *_3DFramebufferRGBA5551;
	
	template<bool ISCUSTOMRENDERINGNEEDED> void _RenderLine_Layer(const u16 l, u16 *dstLine, const size_t dstLineWidth, const size_t dstLineCount);
	template<bool ISCUSTOMRENDERINGNEEDED, size_t CAPTURELENGTH> void _RenderLine_DisplayCapture(const u16 l);
	void _RenderLine_DispCapture_FIFOToBuffer(u16 *fifoLineBuffer);
	
	template<int SOURCESWITCH, size_t CAPTURELENGTH, bool CAPTUREFROMNATIVESRC, bool CAPTURETONATIVEDST>
	void _RenderLine_DispCapture_Copy(const u16 *__restrict src, u16 *__restrict dst, const size_t captureLengthExt, const size_t captureLineCount);
	
	u16 _RenderLine_DispCapture_BlendFunc(const u16 srcA, const u16 srcB, const u8 blendEVA, const u8 blendEVB);
	
#ifdef ENABLE_SSE2
	__m128i _RenderLine_DispCapture_BlendFunc_SSE2(__m128i &srcA, __m128i &srcB, const __m128i &blendEVA, const __m128i &blendEVB);
#endif
	
	template<bool CAPTUREFROMNATIVESRCA, bool CAPTUREFROMNATIVESRCB>
	void _RenderLine_DispCapture_BlendToCustomDstBuffer(const u16 *__restrict srcA, const u16 *__restrict srcB, u16 *__restrict dst, const u8 blendEVA, const u8 blendEVB, const size_t length, size_t l);
	
	template<size_t CAPTURELENGTH, bool CAPTUREFROMNATIVESRCA, bool CAPTUREFROMNATIVESRCB, bool CAPTURETONATIVEDST>
	void _RenderLine_DispCapture_Blend(const u16 *__restrict srcA, const u16 *__restrict srcB, u16 *__restrict dst, const size_t captureLengthExt, const size_t l);
	
public:
	DISPCAPCNT dispCapCnt;
	
	GPUEngineA();
	~GPUEngineA();
	
	virtual void Reset();
	void SetDISPCAPCNT(u32 val);
	GPUDisplayMode GetDisplayMode() const;
	u8 GetVRAMBlock() const;
	FragmentColor* Get3DFramebufferRGBA6665() const;
	u16* Get3DFramebufferRGBA5551() const;
	virtual void SetCustomFramebufferSize(size_t w, size_t h);
		
	template<bool ISCUSTOMRENDERINGNEEDED> void RenderLine(const u16 l, bool skip);
};

class GPUEngineB : public GPUEngineBase
{
protected:
	template<bool ISCUSTOMRENDERINGNEEDED> void _RenderLine_Layer(const u16 l, u16 *dstLine, const size_t dstLineWidth, const size_t dstLineCount);
	
public:
	GPUEngineB();
	
	virtual void Reset();
	template<bool ISCUSTOMRENDERINGNEEDED> void RenderLine(const u16 l, bool skip);
};

class NDSDisplay
{
private:
	NDSDisplayID _ID;
	GPUEngineBase *_gpu;
	
public:
	NDSDisplay();
	NDSDisplay(const NDSDisplayID displayID);
	NDSDisplay(const NDSDisplayID displayID, GPUEngineBase *theEngine);
	
	GPUEngineBase* GetEngine();
	void SetEngine(GPUEngineBase *theEngine);
	
	GPUEngineID GetEngineID();
	void SetEngineByID(const GPUEngineID theID);
};

class GPUSubsystem
{
private:
	GPUEngineA *_engineMain;
	GPUEngineB *_engineSub;
	NDSDisplay *_displayMain;
	NDSDisplay *_displayTouch;
	
	bool _willAutoBlitNativeToCustomBuffer;
	VRAM3DUsageProperties _VRAM3DUsage;
	u16 *_customVRAM;
	u16 *_customVRAMBlank;
	
	CACHE_ALIGN u16 _nativeFramebuffer[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2];
	u16 *_customFramebuffer;
	
	NDSDisplayInfo _displayInfo;
	
public:
	GPUSubsystem();
	~GPUSubsystem();
	
	void Reset();
	VRAM3DUsageProperties& GetVRAM3DUsageProperties();
	const NDSDisplayInfo& GetDisplayInfo(); // Frontends need to call this whenever they need to read the video buffers from the emulator core
	void SetDisplayDidCustomRender(NDSDisplayID displayID, bool theState);
	
	GPUEngineA* GetEngineMain();
	GPUEngineB* GetEngineSub();
	NDSDisplay* GetDisplayMain();
	NDSDisplay* GetDisplayTouch();
	
	u16* GetNativeFramebuffer();
	u16* GetNativeFramebuffer(const NDSDisplayID theDisplayID);
	u16* GetCustomFramebuffer();
	u16* GetCustomFramebuffer(const NDSDisplayID theDisplayID);
	
	u16* GetCustomVRAMBuffer();
	u16* GetCustomVRAMBlankBuffer();
	
	size_t GetCustomFramebufferWidth() const;
	size_t GetCustomFramebufferHeight() const;
	void SetCustomFramebufferSize(size_t w, size_t h);
	
	void UpdateVRAM3DUsageProperties();
	
	// Normally, the GPUs will automatically blit their native buffers to the master
	// framebuffer at the end of V-blank so that all rendered graphics are contained
	// within a single common buffer. This is necessary for when someone wants to read
	// the NDS framebuffers, but the reader can only read a single buffer at a time.
	// Certain functions, such as taking screenshots, as well as many frontends running
	// the NDS video displays, require that they read from only a single buffer.
	//
	// However, if SetWillAutoBlitNativeToCustomBuffer() is passed "false", then the
	// frontend becomes responsible for calling GetDisplayInfo() and reading the native
	// and custom buffers properly for each display. If a single buffer is still needed
	// for certain cases, then the frontend must manually call
	// GPUEngineBase::BlitNativeToCustomFramebuffer() for each GPU before reading the
	// master framebuffer.
	bool GetWillAutoBlitNativeToCustomBuffer() const;
	void SetWillAutoBlitNativeToCustomBuffer(const bool willAutoBlit);
	
	void RenderLine(const u16 l, bool skip = false);
	void ClearWithColor(const u16 colorBGRA5551);
};

extern GPUSubsystem *GPU;
extern MMU_struct MMU;

inline FragmentColor MakeFragmentColor(const u8 r, const u8 g, const u8 b, const u8 a)
{
	FragmentColor ret;
	ret.r = r; ret.g = g; ret.b = b; ret.a = a;
	return ret;
}

#endif
