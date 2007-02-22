/*  Copyright (C) 2006 yopyop
	yopyop156@ifrance.com
	yopyop156.ifrance.com

	Copyright (C) 2006-2007 Theo Berkau
	Copyright (C) 2007 shash

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

#ifndef GPU_H
#define GPU_H

#include "ARM9.h"
#include <stdio.h>
#include "mem.h"
#include "registers.h"
#include "FIFO.h"

#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
    this structure is for display control,
    it holds flags for general display
*******************************************************************************/

struct _DISPCNT
{
/*0*/    unsigned BG_Mode:3;         // A+B:
/*3*/    unsigned BG0_3D:1;          // A  : 0=2D,         1=3D
/*4*/    unsigned OBJ_Tile_1D:1;     // A+B: 0=2D (32KB),  1=1D (32..256KB)
/*5*/    unsigned OBJ_BMP_2D_dim:1;  // A+B: 0=128x512,    1=256x256 pixels
/*6*/    unsigned OBJ_BMP_mapping:1; // A+B: 0=2D (128KB), 1=1D (128..256KB)

                                    // 7-15 same as GBA
/*7*/    unsigned ForceBlank:1;      // A+B: 
/*8*/    unsigned BG0_Enable:1;        // A+B: 0=disable, 1=Enable
/*9*/   unsigned BG1_Enable:1;      // A+B: 0=disable, 1=Enable
/*10*/  unsigned BG2_Enable:1;      // A+B: 0=disable, 1=Enable
/*11*/  unsigned BG3_Enable:1;      // A+B: 0=disable, 1=Enable
/*12*/  unsigned OBJ_Enable:1;      // A+B: 0=disable, 1=Enable
/*13*/  unsigned Win0_Enable:1;     // A+B: 0=disable, 1=Enable
/*14*/  unsigned Win1_Enable:1;     // A+B: 0=disable, 1=Enable
/*15*/  unsigned WinOBJ_Enable:1;   // A+B: 0=disable, 1=Enable

/*16*/  unsigned DisplayMode:2;     // A+B: coreA(0..3) coreB(0..1) GBA(Green Swap)
                                    // 0=off (white screen)
                                    // 1=on (normal BG & OBJ layers)
                                    // 2=VRAM display (coreA only)
                                    // 3=RAM display (coreA only, DMA transfers)
                    
/*18*/  unsigned VRAM_Block:2;            // A  : VRAM block (0..3=A..D)
/*20*/  unsigned OBJ_Tile_1D_Bound:2;     // A+B: 
/*22*/  unsigned OBJ_BMP_1D_Bound:1;      // A  : 
/*23*/  unsigned OBJ_HBlank_process:1;    // A+B: OBJ processed during HBlank (GBA bit5)
/*24*/  unsigned CharacBase_Block:3;      // A  : Character Base (64K step)
/*27*/  unsigned ScreenBase_Block:3;      // A  : Screen Base (64K step)
/*30*/  unsigned ExBGxPalette_Enable:1;   // A+B: 0=disable, 1=Enable BG extended Palette
/*31*/  unsigned ExOBJPalette_Enable:1;   // A+B: 0=disable, 1=Enable OBJ extended Palette
};

typedef union 
{
    struct _DISPCNT bits;
    u32 val;
} DISPCNT;
#define BGxENABLED(cnt,num)    ((num<8)? ((cnt.val>>8) & num):0)




/*******************************************************************************
    this structure is for display control of a specific layer,
    there are 4 background layers
    their priority indicate which one to draw on top of the other
    some flags indicate special drawing mode, size, FX
*******************************************************************************/

struct _BGxCNT 
{
/* 0*/ unsigned Priority:2;            // 0..3=high..low
/* 2*/ unsigned CharacBase_Block:4;    // individual character base offset (n*16KB)
/* 6*/ unsigned Mosaic_Enable:1;       // 0=disable, 1=Enable mosaic
/* 7*/ unsigned Palette_256:1;         // 0=16x16, 1=1*256 palette
/* 8*/ unsigned ScreenBase_Block:5;    // individual screen base offset (text n*2KB, BMP n*16KB)
/*13*/ unsigned PaletteSet_Wrap:1;     // BG0 extended palette set 0=set0, 1=set2
                                       // BG1 extended palette set 0=set1, 1=set3
                                       // BG2 overflow area wraparound 0=off, 1=wrap
                                       // BG3 overflow area wraparound 0=off, 1=wrap
/*14*/ unsigned ScreenSize:2;          // text    : 256x256 512x256 256x512 512x512
                                       // x/rot/s : 128x128 256x256 512x512 1024x1024
                                       // bmp     : 128x128 256x256 512x256 512x512
                                       // large   : 512x1024 1024x512 - -
};


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
    u16 BGxPA;
    u16 BGxPB;
    u16 BGxPC;
    u16 BGxPD;
    u32 BGxX;
    u32 BGxY;
} BGxPARMS;


/*******************************************************************************
    this structure is for windows parameters
*******************************************************************************/

typedef struct {
    u16 WIN0H;
    u16 WIN1H;
    u16 WIN0V;
    u16 WIN1V;
    u16 WININ;
    u16 WINOUT;
} WINCNT;

/*******************************************************************************
    this structure is for miscellanous settings
*******************************************************************************/

typedef struct {
    u16 MOSAIC;
    u16 unused1;
    u16 BLDCNT;
    u16 BLDALPHA;
    u16 BLDY;
    u16 unused2;
    u16 unused3;
    u16 unused4;
    u16 unused5;
    u16 unused6;
} MISCCNT;


/*******************************************************************************
    this structure is for 3D settings
*******************************************************************************/

struct _DISP3DCNT
{
/* 0*/ unsigned EnableTexMapping:1;    //
/* 1*/ unsigned PolygonShading:1;      // 0=Toon Shading, 1=Highlight Shading
/* 2*/ unsigned EnableAlphaTest:1;     // see ALPHA_TEST_REF
/* 3*/ unsigned EnableAlphaBlending:1; // see various Alpha values
/* 4*/ unsigned EnableAntiAliasing:1;  //
/* 5*/ unsigned EnableEdgeMarking:1;   // see EDGE_COLOR
/* 6*/ unsigned FogOnlyAlpha:1;        // 0=Alpha and Color, 1=Only Alpha (see FOG_COLOR)
/* 7*/ unsigned EnableFog:1;           // Fog Master Enable 
/* 8*/ unsigned FogShiftSHR:4;         // 0..10 SHR-Divider (see FOG_OFFSET)
/*12*/ unsigned AckColorBufferUnderflow:1; // Color Buffer RDLINES Underflow (0=None, 1=Underflow/Acknowledge)
/*13*/ unsigned AckVertexRAMOverflow:1;    // Polygon/Vertex RAM Overflow    (0=None, 1=Overflow/Acknowledge)
/*14*/ unsigned RearPlaneMode:1;       // 0=Blank, 1=Bitmap
/*15*/ unsigned :17;
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

struct _DISPCAPCNT
{
/* 0*/ unsigned BlendFactor_A:5;     // 0..16 = Blending Factor for Source A
/* 5*/ unsigned :3;                  //
/* 8*/ unsigned BlendFactor_B:5;     // 0..16 = Blending Factor for Source B
/*13*/ unsigned :3;                  //
/*16*/ unsigned VRAM_Write_Block:2;  // 0..3 = VRAM A..D
/*18*/ unsigned VRAM_Write_Offset:2; // n x 0x08000
/*20*/ unsigned Capture_Size:2;      // 0=128x128, 1=256x64, 2=256x128, 3=256x192 dots
/*22*/ unsigned :2;                  //
/*24*/ unsigned Source_A:1;          // 0=Graphics Screen BG+3D+OBJ, 1=3D Screen
/*25*/ unsigned Source_B:1;          // 0=VRAM, 1=Main Memory Display FIFO
/*26*/ unsigned VRAM_Read_Offset:2;  // n x 0x08000
/*28*/ unsigned :1;                  //
/*29*/ unsigned Capture_Source:2;    // 0=Source A, 1=Source B, 2/3=Sources A+B blended
/*31*/ unsigned Capture_Enable:1;    // 0=Disable/Ready, 1=Enable/Busy
};

typedef union 
{
    struct _DISPCAPCNT bits;
    u32 val;
} DISPCAPCNT;



/*******************************************************************************
    this structure is to represent global FX
    applied to each pixel (each color of x555 is expanded to 6 bits)

    Lighten : New = Old + (63-Old) * Factor/16
    Darken  : New = Old - Old      * Factor/16
*******************************************************************************/

struct _MASTER_BRIGHT
{
/* 0*/ unsigned Factor:4;   // combine with (Factor / 16) of white/black
/* 4*/ unsigned FactorEx:1; // if true use white or black
/* 5*/ unsigned :9;
/*14*/ unsigned Mode:2;     // 0=off, 1=Lighten, 2=Darken, 3=?
};

typedef union 
{
    struct _MASTER_BRIGHT bits;
    u16 val;
} MASTER_BRIGHT;

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
    WINCNT dispx_WINCNT;              // 0x0400x040
    MISCCNT dispx_MISC;               // 0x0400x04C
    DISP3DCNT dispA_DISP3DCNT;        // 0x04000060
    DISPCAPCNT dispA_DISPCAPCNT;      // 0x04000064
    u32 dispA_DISPMMEMFIFO;           // 0x04000068
    MASTER_BRIGHT dispx_MASTERBRIGHT; // 0x0400x06C
} REG_DISPx;











#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif

typedef BOOL (*fun_gl_Begin) (int screen);
typedef void (*fun_gl_End) (int screen);
// the GUI should use this function prior to all gl calls
// if call to beg succeeds opengl draw 
void register_gl_fun(fun_gl_Begin beg,fun_gl_End end);

#define GPU_MAIN	0
#define GPU_SUB		1

/* human readable bitmask names */
#define ADDRESS_STEP_512B	   0x00200
#define ADDRESS_STEP_1KB		0x00400
#define ADDRESS_STEP_2KB		0x00800
#define ADDRESS_STEP_4KB		0x01000
#define ADDRESS_STEP_8KB		0x02000
#define ADDRESS_STEP_16KB	   0x04000
#define ADDRESS_STEP_32KB	   0x08000
#define ADDRESS_STEP_64kB	   0x10000

struct _TILEENTRY
{
/* 0*/	unsigned TileNum:10;
/*12*/	unsigned HFlip:1;	// HORIZONTAL FLIP (left<-->right)
/*13*/	unsigned VFlip:1;	// VERTICAL FLIP (top<-->bottom)
/*14*/	unsigned Palette:4;
};
typedef union 
{
	struct _TILEENTRY bits;
	u16 val;
} TILEENTRY;

struct _ROTOCOORD
{
/* 0*/	unsigned Fraction:8;
/* 8*/	  signed Integer:24;
// /*28*/	unsigned :4;
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
	unsigned red:5;
	unsigned green:5;
	unsigned blue:5;
	unsigned alpha:1;	// sometimes it is unused (pad)
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






/*
	this structure is for Sprite description,
	it holds flags & transformations for 1 sprite
	(max 128 OBJs / screen)
ref: http://www.bottledlight.com/ds/index.php/Video/Sprites
*/

typedef struct
{
// attr0
/* 0*/	unsigned Y:8;
/* 8*/	unsigned RotScale:2; // (00: Normal, 01: Rot/scale, 10: Disabled, 11: Double-size rot/scale)
/*10*/	unsigned Mode:2;	 // (00: Normal, 01: Transparent, 10: Object window, 11: Bitmap)
/*12*/	unsigned Mosaic:1;   // (1: Enabled)
/*13*/	unsigned Depth:1;	// (0: 16, 1: 256)
/*14*/	unsigned Shape:2;	// (00: Square, 01: Wide, 10: Tall, 11: Illegal)
// attr1
/* 0*/	signed   X:9;
/* 9*/	unsigned RotScalIndex:3; // Rot/scale matrix index 
/*12*/	unsigned HFlip:1;
/*13*/	unsigned VFlip:1;
/*14*/	unsigned Size:2;
// attr2
/* 0*/	unsigned TileIndex:10;
/*10*/	unsigned Priority:2;
/*12*/	unsigned PaletteIndex:4;
// attr3
	unsigned attr3:16;
} _OAM_;

typedef struct
{
	 u16 attr0;
	 u16 attr1;
	 u16 attr2;
	 u16 attr3;
} OAM;



typedef struct
{
	 s16 x;
	 s16 y;
} size;


/*
	these structures are for window description,
	windows are square regions and can "subclass"
	background layers or object layers (i.e window controls the layers)

	screen
	|
	+-- Window0/Window1/OBJwindow/OutOfWindows
		|
		+-- BG0/BG1/BG2/BG3/OBJ
*/

typedef union windowdim_t
{
	struct
	{
/* 0*/	unsigned end:8;
/* 8*/	unsigned start:8;
	} bits ;
	unsigned short val ;
} windowdim_t ;

typedef union windowcnt_t
{
	struct
	{
/* 0*/  unsigned WIN0_BG0_Enable:1;
/* 1*/  unsigned WIN0_BG1_Enable:1;
/* 2*/  unsigned WIN0_BG2_Enable:1;
/* 3*/  unsigned WIN0_BG3_Enable:1;
/* 4*/  unsigned WIN0_OBJ_Enable:1;
/* 5*/  unsigned WIN0_Effect_Enable:1;
/* 6*/  unsigned :2;
/* 8*/  unsigned WIN1_BG0_Enable:1;
/* 9*/  unsigned WIN1_BG1_Enable:1;
/*10*/  unsigned WIN1_BG2_Enable:1;
/*11*/  unsigned WIN1_BG3_Enable:1;
/*12*/  unsigned WIN1_OBJ_Enable:1;
/*13*/  unsigned WIN1_Effect_Enable:1;
/*14*/  unsigned :2;
	} bits ;
	struct
	{
		unsigned char low ;
		unsigned char high ;
	} bytes ;
	struct
	{
		unsigned win0_en:5;
		unsigned :3;
		unsigned win1_en:5;
		unsigned :3;
	} windows ;
	unsigned short val ;
} windowcnt_t ;



/*
	this structure holds information
	for rendering.
*/

#define NB_PRIORITIES	4
#define NB_BG		4
typedef struct 
{
	u8 BGs[NB_BG], nbBGs;
	u8 PixelsX[256];
	// doh ! yoda says : 256 pixels we can have...
	u16 nbPixelsX;
} itemsForPriority_t;


typedef struct _GPU GPU;

struct _GPU
{
	// some structs are becoming redundant
	// some functions too (no need to recopy some vars as it is done by MMU)
	REG_DISPx * dispx_st;
		
//	DISPCNT dispCnt;
	DISPCAPCNT dispCapCnt;
//	BGxCNT  bgCnt[4];
//	MASTER_BRIGHT masterBright;
	BOOL LayersEnable[5];
	itemsForPriority_t itemsForPriority[NB_PRIORITIES];
	u8 sprWin[256*2][256];

#define BGBmpBB BG_bmp_ram
#define BGChBB BG_tile_ram
		 
	u8 *(BG_bmp_ram[4]);
	u8 *(BG_tile_ram[4]);
	u8 *(BG_map_ram[4]);
			
	u8 BGExtPalSlot[4];
	u32 BGSize[4][2];
	u16 BGSX[4];
	u16 BGSY[4];
	
	s32 BGX[4];
	s32 BGY[4];
	s16 BGPA[4];
	s16 BGPB[4];
	s16 BGPC[4];
	s16 BGPD[4];
	
	u8 lcd;
	u8 core;
	
	u8 dispMode;
	u8 vramBlock;

	BOOL dispBG[4];
	BOOL dispOBJ;
	
	OAM * oam;
	u8 * sprMem;
	u8 sprBoundary;
	u8 sprBMPBoundary;
	u8 sprBMPMode;
	u32 sprEnable ;
	
	u16 BLDCNT ;
	u16 BLDALPHA ;
	u16 BLDY ;
	u16 MOSAIC ;

	windowdim_t WINDOW_XDIM[2] ;
	windowdim_t WINDOW_YDIM[2] ;
	windowcnt_t WINDOW_INCNT ;
	windowcnt_t WINDOW_OUTCNT ;

	void (*spriteRender)(GPU * gpu, u16 l, u8 * dst, u8 * prioTab);
};

extern u8 GPU_screen[4*256*192];


GPU * GPU_Init(u8 l);
void GPU_Reset(GPU *g, u8 l);
void GPU_DeInit(GPU *);

void textBG(GPU * gpu, u8 num, u8 * DST);
void rotBG(GPU * gpu, u8 num, u8 * DST);
void extRotBG(GPU * gpu, u8 num, u8 * DST);
void sprite1D(GPU * gpu, u16 l, u8 * dst, u8 * prioTab);
void sprite2D(GPU * gpu, u16 l, u8 * dst, u8 * prioTab);

extern short sizeTab[4][4][2];
extern size sprSizeTab[4][4];
extern s8 mode2type[8][4];
extern void (*modeRender[8][4])(GPU * gpu, u8 num, u16 l, u8 * DST);

typedef struct {
	GPU * gpu;
	u16 offset;
} NDS_Screen;

extern NDS_Screen MainScreen;
extern NDS_Screen SubScreen;

int Screen_Init(int coreid);
void Screen_Reset(void);
void Screen_DeInit(void);

extern MMU_struct MMU;



#define GFXCORE_DEFAULT		 -1
#define GFXCORE_DUMMY		   0

#define GFXCORE_FULLSCREEN	  (1 << 0)

typedef struct
{
   int id; // ID number for core(see above defines)
   const char *Name; // Name of core
   int flags; // What features the core supports(full screen, etc.)
   int (*Init)(); // Initializes stuff related to core
   void (*DeInit)(); // Deinitializes stuff related to core
   void (*Resize)(int width, int height, BOOL fullscreen); // Resizes window or fullscreen
   void (*OnScreenText)(char *string, ...); // For handling save state messages, etc. 
} GraphicsInterface_struct;

extern GraphicsInterface_struct GFXDummy;

void GPU_setVideoProp(GPU *, u32 p);
void GPU_setBGProp(GPU *, u16 num, u16 p);

void GPU_scrollX(GPU *, u8 num, u16 v);
void GPU_scrollY(GPU *, u8 num, u16 v);

// no more useful if using REG_DISPx
void GPU_setXH(GPU *, u8 num, u16 v);
void GPU_setXL(GPU *, u8 num, u16 v);
void GPU_setYH(GPU *, u8 num, u16 v);
void GPU_setYL(GPU *, u8 num, u16 v);
void GPU_setPA(GPU *, u8 num, u16 v);
void GPU_setPB(GPU *, u8 num, u16 v);
void GPU_setPC(GPU *, u8 num, u16 v);
void GPU_setPD(GPU *, u8 num, u16 v);

void GPU_scrollXY(GPU *, u8 num, u32 v);
// no more useful if using REG_DISPx
void GPU_setX(GPU *, u8 num, u32 v);
void GPU_setY(GPU *, u8 num, u32 v);
void GPU_setPAPB(GPU *, u8 num, u32 v);
void GPU_setPCPD(GPU *, u8 num, u32 v);
	   
void GPU_setBLDCNT(GPU *gpu, u16 v) ;
void GPU_setBLDALPHA(GPU *gpu, u16 v) ;
void GPU_setBLDY(GPU *gpu, u16 v) ;
void GPU_setMOSAIC(GPU *gpu, u16 v) ;

void GPU_setWINDOW_XDIM(GPU *gpu, u16 v, u8 num) ;
void GPU_setWINDOW_YDIM(GPU *gpu, u16 v, u8 num) ;
void GPU_setWINDOW_XDIM_Component(GPU *gpu, u8 v, u8 num) ;
void GPU_setWINDOW_YDIM_Component(GPU *gpu, u8 v, u8 num) ;

void GPU_setWINDOW_INCNT(GPU *gpu, u16 v) ;
void GPU_setWINDOW_OUTCNT(GPU *gpu, u16 v) ;
void GPU_setWINDOW_INCNT_Component(GPU *gpu, u8 v,u8 num) ;
void GPU_setWINDOW_OUTCNT_Component(GPU *gpu, u8 v,u8 num) ;

void GPU_setMASTER_BRIGHT (GPU *gpu, u16 v);

void GPU_remove(GPU *, u8 num);
void GPU_addBack(GPU *, u8 num);

int GPU_ChangeGraphicsCore(int coreid);

void GPU_set_DISPCAPCNT(GPU * gpu, u32 val) ;
void GPU_ligne(NDS_Screen * screen, u16 l) ;

#ifdef __cplusplus
}
#endif

#endif

