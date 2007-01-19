/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright (C) 2006 Theo Berkau

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


#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif


#define GPU_MAIN	0
#define GPU_SUB		1

/* human readable bitmask names */
#define ADDRESS_STEP_512B       0x00200
#define ADDRESS_STEP_1KB        0x00400
#define ADDRESS_STEP_2KB        0x00800
#define ADDRESS_STEP_4KB        0x01000
#define ADDRESS_STEP_8KB        0x02000
#define ADDRESS_STEP_16KB       0x04000
#define ADDRESS_STEP_32KB       0x08000
#define ADDRESS_STEP_64kB       0x10000


/*
	this structure is for display control,
	it holds flags for general display
*/

struct _DISPCNT
{
/*0*/	unsigned BG_Mode:3;		// A+B:
/*3*/	unsigned BG0_3D:1;		// A  : 0=2D,         1=3D
/*4*/	unsigned OBJ_Tile_1D:1;		// A+B: 0=2D (32KB),  1=1D (32..256KB)
/*5*/	unsigned OBJ_BMP_2D_dim:1;	// A+B: 0=128x512,    1=256x256 pixels
/*6*/	unsigned OBJ_BMP_mapping:1;	// A+B: 0=2D (128KB), 1=1D (128..256KB)

// 7-15 same as GBA
/*7*/	unsigned ForceBlank:1;		// A+B: 
/*8*/	unsigned BG0_Enable:1;		// A+B: 0=disable, 1=Enable
/*9*/	unsigned BG1_Enable:1;		// A+B: 0=disable, 1=Enable
/*10*/	unsigned BG2_Enable:1;		// A+B: 0=disable, 1=Enable
/*11*/	unsigned BG3_Enable:1;		// A+B: 0=disable, 1=Enable
/*12*/	unsigned OBJ_Enable:1;		// A+B: 0=disable, 1=Enable
/*13*/	unsigned Win0_Enable:1;		// A+B: 0=disable, 1=Enable
/*14*/	unsigned Win1_Enable:1;		// A+B: 0=disable, 1=Enable
/*15*/	unsigned WinOBJ_Enable:1;	// A+B: 0=disable, 1=Enable

/*16*/	unsigned DisplayMode:2;		// A+B: coreA(0..3) coreB(0..1) GBA(Green Swap)
					// 0=off (white screen)
					// 1=on (normal BG & OBJ layers)
					// 2=VRAM display (coreA only)
					// 3=RAM display (coreA only, DMA transfers)
					
/*18*/	unsigned VRAM_Block:2;		// A  : VRAM block (0..3=A..D)
/*20*/	unsigned OBJ_Tile_1D_Bound:2;	// A+B: 
/*22*/	unsigned OBJ_BMP_1D_Bound:1;	// A  : 
/*23*/	unsigned OBJ_HBlank_process:1;	// A+B: OBJ processed during HBlank (GBA bit5)
/*24*/	unsigned CharacBase_Block:3;	// A  : Character Base (64K step)
/*27*/	unsigned ScreenBase_Block:3;	// A  : Screen Base (64K step)
/*30*/	unsigned ExBGxPalette_Enable:1;	// A+B: 0=disable, 1=Enable BG extended Palette
/*31*/	unsigned ExOBJPalette_Enable:1;	// A+B: 0=disable, 1=Enable OBJ extended Palette
};

typedef union 
{
	struct _DISPCNT bits;
	u32 val;
} DISPCNT;
#define BGxENABLED(cnt,num)	((num<8)? ((cnt.val>>8) & num):0)



/*
	this structure is for color representation,
	it holds 5 meaningful bits per color channel (red,green,blue)
	and      1 meaningful bit for alpha representation
	this bit can be unused or used for special FX
*/

struct _COLOR { // abgr x555
	unsigned red:5;
	unsigned green:5;
	unsigned blue:5;
	unsigned alpha:1;	// sometimes it is unused (pad)
};

typedef union 
{
	struct _COLOR bits;
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
	this structure is for display control of a specific layer,
	there are 4 background layers
	their priority indicate which one to draw on top of the other
	some flags indicate special drawing mode, size, FX
*/


struct _BGxCNT 
{
/*0*/	unsigned Priority:2;		// 0..3=high..low
/*2*/	unsigned CharacBase_Block:4;	// individual character base offset (n*16KB)
/*6*/	unsigned Mosaic_Enable:1;	// 0=disable, 1=Enable mosaic
/*7*/	unsigned Palette_256:1;		// 0=16x16, 1=1*256 palette
/*8*/	unsigned ScreenBase_Block:5;	// individual screen base offset (text n*2KB, BMP n*16KB)
/*13*/	unsigned PaletteSet_Wrap:1;	// BG0 extended palette set 0=set0, 1=set2
					// BG1 extended palette set 0=set1, 1=set3
					// BG2 overflow area wraparound 0=off, 1=wrap
					// BG3 overflow area wraparound 0=off, 1=wrap
/*14*/	unsigned ScreenSize:2;		// text    : 256x256 512x256 256x512 512x512
					// x/rot/s : 128x128 256x256 512x512 1024x1024
					// bmp     : 128x128 256x256 512x256 512x512
					// large   : 512x1024 1024x512 - -
};

typedef union 
{
	struct _BGxCNT bits;
	u16 val;
} BGxCNT;

/*
	this structure is to represent global FX
	applied to a pixel 

	Factor (5bits values 0-16, values >16 same as 16)
// damdoum : then the 4th bit use is questionnable ?

	Lighten : New = Old + (63-Old) * Factor/16
	Darken  : New = Old - Old      * Factor/16
*/

struct _MASTER_BRIGHT
{
/*0*/	unsigned Factor:4;	// brightnessFactor = Factor | FactorEx << 4
/*4*/	unsigned FactorEx:1;	//
/*5*/	unsigned :9;
/*14*/	unsigned Mode:2;	// 0=off, 1=Lighten, 2=Darken, 3=?
};

typedef union 
{
	struct _MASTER_BRIGHT bits;
	u16 val;
} MASTER_BRIGHT;



/*
	this structure is for Sprite description,
	it holds flags & transformations for 1 sprite
	(max 128 OBJs / screen)
ref: http://www.bottledlight.com/ds/index.php/Video/Sprites
*/

typedef struct
{
// attr0
/*0*/	unsigned Y:8;
/*8*/	unsigned RotScale:2; // (00: Normal, 01: Rot/scale, 10: Disabled, 11: Double-size rot/scale)
/*10*/	unsigned Mode:2;     // (00: Normal, 01: Transparent, 10: Object window, 11: Bitmap)
/*12*/	unsigned Mosaic:1;   // (1: Enabled)
/*13*/	unsigned Depth:1;    // (0: 16, 1: 256)
/*14*/	unsigned Shape:2;    // (00: Square, 01: Wide, 10: Tall, 11: Illegal)
// attr1
/*0*/	signed   X:9;
/*9*/	unsigned RotScalIndex:3; // Rot/scale matrix index 
/*12*/	unsigned HFlip:1;
/*13*/	unsigned VFlip:1;
/*14*/	unsigned Size:2;
// attr2
/*0*/	unsigned TileIndex:10;
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
	u8 PixelsX[256], nbPixelsX;
} itemsForPriority_t;


typedef struct _GPU GPU;

struct _GPU
{
	DISPCNT dispCnt;
	BGxCNT  bgCnt[4];
	MASTER_BRIGHT masterBright;
	BOOL LayersEnable[5];
	itemsForPriority_t itemsForPriority[NB_PRIORITIES];
	u8 sprWin[192][256];

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
	
	u8 nbBGActif;
	u8 BGIndex[4];
	u8 ordre[4];
	BOOL dispBG[4];
	BOOL dispOBJ;
	
	OAM * oam;
	u8 * sprMem;
	u8 sprBlock;
	u8 sprBMPBlock;
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
} Screen;

extern Screen MainScreen;
extern Screen SubScreen;

void Screen_Init(void);
void Screen_Reset(void);
void Screen_DeInit(void);

extern MMU_struct MMU;

static INLINE void GPU_ligne(Screen * screen, u16 l)
{
	GPU * gpu = screen->gpu;
	u8 * dst =  GPU_screen + (screen->offset + l) * 512;
	u8 * mdst =  GPU_screen + (MainScreen.offset + l) * 512;
	u8 * sdst =  GPU_screen + (SubScreen.offset + l) * 512;
	itemsForPriority_t * item;
	u8 spr[512];
	u8 sprPrio[256];
	u8 bgprio,prio;
	int i;
	int vram_bank;
	u8 i8;
	u16 i16;
	u32 c;
	u8 n,p;
	
	u8 *dest;
	
	/* initialize the scanline black */
	/* not doing this causes invalid colors when all active BGs are prevented to draw at some place */
	memset(dst,0,256*2) ;

	// This could almost be changed to use function pointers
	switch (gpu->dispMode)
	{
		case 1: // Display BG and OBJ layers
			break;
		case 0: // Display Off(Display white)
			for (i=0; i<256; i++)
				T2WriteWord(dst, i << 1, 0x7FFF);
			return;
		case 2: // Display framebuffer
		{
		//addition from Normatt
				/* we only draw one of the VRAM blocks */
				vram_bank = gpu->dispCnt.bits.VRAM_Block ;
				
//				if(!(gpu->lcd)) dest = mdst; else dest = sdst;
				dest = dst ;
				{
					int ii = l * 256 * 2;
					for (i=0; i<(256 * 2); i+=2)
					{
						u8 * vram ;
						if (MMU.vram_mode[vram_bank] & 4)
						{
							vram = ARM9Mem.ARM9_LCD + (MMU.vram_mode[vram_bank] & 3) * 0x20000;
						} else
						{
							vram = ARM9Mem.ARM9_ABG + MMU.vram_mode[vram_bank] * 0x20000;
						}

						T2WriteWord(dest, i, T1ReadWord(vram, ii));
						ii+=2;
					}
					return;
				}
		}
			return;
		case 3:
	// Read from FIFO MAIN_MEMORY_DISP_FIFO, two pixels at once format is x555, bit15 unused
	// Reference:  http://nocash.emubase.de/gbatek.htm#dsvideocaptureandmainmemorydisplaymode
	// (under DISP_MMEM_FIFO)
			for (i=0; i<256;) {
				c = FIFOValue(MMU.fifos + MAIN_MEMORY_DISP_FIFO);
				T2WriteWord(dst, i << 1, c&0xFFFF); i++;
				T2WriteWord(dst, i << 1, c>>16); i++;
			}
			return;
	}


	c = T1ReadWord(ARM9Mem.ARM9_VMEM, gpu->core * 0x400);
	c |= (c<<16);
	
	// init background color & priorities
	for(i8 = 0; i8< 128; ++i8)
	{
		T2WriteLong(dst, i8 << 2, c);
		T2WriteLong(spr, i8 << 2, c);
		// we init the sprites with priority 4 (if they keep it = unprocessed)
		T1WriteWord(sprPrio, i8 << 1, (4 << 8) | (4));
		T1WriteWord(gpu->sprWin[l], i8 << 1, 0);
	}
	
	// init pixels priorities
	for (i=0;i<NB_PRIORITIES;i++) {
		gpu->itemsForPriority[i].nbPixelsX = 0;
	}

	// for all the pixels in the line
	if (gpu->LayersEnable[4]) {
		gpu->spriteRender(gpu, l, spr, sprPrio);
		for(i= 0; i<256; i++) {
			// assign them to the good priority item
			prio = sprPrio[i];
	
			// render 1 time, but prio 4 = isn't processed further
			if (prio >=4) continue;
			T2WriteWord(dst, i << 1, T2ReadWord(spr, i << 1));
			
			item = &(gpu->itemsForPriority[prio]);
			item->PixelsX[item->nbPixelsX]=i;
			item->nbPixelsX++;
		}
	}

	// paint lower priorities fist
	// then higher priorities on top
	for(prio=NB_PRIORITIES; prio > 0; )
	{
		prio--;
		item = &(gpu->itemsForPriority[prio]);
		// render BGs
		for (i=0; i < item->nbBGs; i++) {
			i16 = item->BGs[i];
			if (gpu->LayersEnable[i16])
			modeRender[gpu->dispCnt.bits.BG_Mode][i16](gpu, i16, l, dst);
		}
		// render sprite Pixels
		for (i=0; i < item->nbPixelsX; i++) {
			i16=item->PixelsX[i];
			T2WriteWord(dst, i16 << 1, T2ReadWord(spr, i16 << 1));
		}
	}


// Apply final brightness adjust (MASTER_BRIGHT)
//  Reference: http://nocash.emubase.de/gbatek.htm#dsvideo (Under MASTER_BRIGHTNESS)
/* Mightymax> it should be more effective if the windowmanager applies brightness when drawing */
/* it will most likly take acceleration, while we are stuck here with CPU power */

	 switch (gpu->masterBright.bits.Mode)
	 {
		// Disabled
		case 0:
			break;

		// Bright up
		case 1:
		{
			unsigned int masterBrightFactor = gpu->masterBright.bits.Factor;

			if (gpu->masterBright.bits.FactorEx)
			{
				/* the formular would create only white, as (r + (31-r)) = 31 */
				/* white = enable all bits */

				/* damdoum : well, i think the formula was with 63! 
				   so (r + (63-r)) = 63 & 31 = 31 = still white*/

				memset(dst,0xFF, 256*2 /* sizeof(COLOR) */) ;
			} else
			{
				if (!masterBrightFactor) break ;    /* when we wont do anything, we dont need to loop */

				for(i16 = 0; i16 < 256; ++i16)
				{
 					COLOR dstColor;
					u8 base ;
 					unsigned int	r,g,b; // get components, 5bit each
 					dstColor.val = T1ReadWord(dst, i16 << 1);
 					r = dstColor.bits.red;
 					g = dstColor.bits.green;
 					b = dstColor.bits.blue;
 					// Bright up and clamp to 5bit <-- automatic
					base = /*gpu->masterBright.bits.FactorEx? 63:*/ 31 ;
					dstColor.bits.red   = r + ((base-r)*masterBrightFactor)/16;
					dstColor.bits.green = g + ((base-g)*masterBrightFactor)/16;
					dstColor.bits.blue  = b + ((base-b)*masterBrightFactor)/16;
 					T2WriteWord (dst, i16 << 1, dstColor.val);
				}
			}
			break;
		}

		// Bright down
		case 2:
		{
/*
	NOTE: gbatek (in the reference above) seems to expect 6bit values 
	per component, but as desmume works with 5bit per component, 
			we use 31 as top, instead of 63. Testing it on a few games, 
			using 63 seems to give severe color wraping, and 31 works
			nicely, so for now we'll just that, until proven wrong.

	i have seen pics of pokemon ranger getting white with 31, with 63 it is nice.
	it could be pb of alpha or blending or...

	MightyMax> created a test NDS to check how the brightness values work,
	and 31 seems to be correct. FactorEx is a override for max brighten/darken
	See: http://mightymax.org/gfx_test_brightness.nds
	The Pokemon Problem could be a problem with 8/32 bit writes not recognized yet,
	i'll add that so you can check back.
.
*/
			unsigned int    masterBrightFactor = gpu->masterBright.bits.Factor ;
 
			if (gpu->masterBright.bits.FactorEx)
			{
				/* the formular would create only black, as (r - r) = 0 */
				/* black = disable all bits */
				memset(dst,0, 256*2 /* sizeof(COLOR) */) ;
			} else
			{
			if (!masterBrightFactor) break ;    /* when we wont do anything, we dont need to loop */
 
				for(i16 = 0; i16 < 256; ++i16)
				{
					COLOR dstColor;
					unsigned int	r,g,b; // get components, 5bit each
					dstColor.val = T1ReadWord(dst, i16 << 1);
					r = dstColor.bits.red;
					g = dstColor.bits.green;
					b = dstColor.bits.blue;
					// Bright up and clamp to 5bit <- automatic
					dstColor.bits.red   = r - (r*masterBrightFactor)/16;
					dstColor.bits.green = g - (g*masterBrightFactor)/16;
					dstColor.bits.blue  = b - (b*masterBrightFactor)/16;
					T2WriteWord (dst, i16 << 1, dstColor.val);
				}
			}
			break;
		}

		// Reserved
		case 3:
			break;
	 }
}
 
void GPU_setVideoProp(GPU *, u32 p);
void GPU_setBGProp(GPU *, u16 num, u16 p);
void GPU_scrollX(GPU *, u8 num, u16 v);
void GPU_scrollY(GPU *, u8 num, u16 v);
void GPU_scrollXY(GPU *, u8 num, u32 v);

void GPU_setX(GPU *, u8 num, u32 v);
void GPU_setXH(GPU *, u8 num, u16 v);
void GPU_setXL(GPU *, u8 num, u16 v);
void GPU_setY(GPU *, u8 num, u32 v);
void GPU_setYH(GPU *, u8 num, u16 v);
void GPU_setYL(GPU *, u8 num, u16 v);
void GPU_setPA(GPU *, u8 num, u16 v);
void GPU_setPB(GPU *, u8 num, u16 v);
void GPU_setPC(GPU *, u8 num, u16 v);
void GPU_setPD(GPU *, u8 num, u16 v);
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
void GPU_toggleOBJ(GPU *, u8 disp);

#ifdef __cplusplus
}
#endif

#endif

