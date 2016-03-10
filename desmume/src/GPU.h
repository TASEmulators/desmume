/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 Theo Berkau
	Copyright (C) 2007 shash
	Copyright (C) 2009-2016 DeSmuME team

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

#ifdef ENABLE_SSSE3
#include <tmmintrin.h>
#endif

class GPUEngineBase;
class EMUFILE;
struct MMU_struct;

//#undef FORCEINLINE
//#define FORCEINLINE

#define GPU_FRAMEBUFFER_NATIVE_WIDTH	256
#define GPU_FRAMEBUFFER_NATIVE_HEIGHT	192

#define GPU_VRAM_BLOCK_LINES			256
#define GPU_VRAM_BLANK_REGION_LINES		544

void gpu_savestate(EMUFILE* os);
bool gpu_loadstate(EMUFILE* is, int size);

typedef void (*PixelLookupFunc)(const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *__restrict pal, u8 &outIndex, u16 &outColor);

enum PaletteMode
{
	PaletteMode_16x16		= 0,
	PaletteMode_1x256		= 1
};

enum OBJMode
{
	OBJMode_Normal			= 0,
	OBJMode_Transparent		= 1,
	OBJMode_Window			= 2,
	OBJMode_Bitmap			= 3
};

enum OBJShape
{
	OBJShape_Square			= 0,
	OBJShape_Horizontal		= 1,
	OBJShape_Vertical		= 2,
	OBJShape_Prohibited		= 3
};

enum DisplayCaptureSize
{
	DisplayCaptureSize_128x128	= 0,
	DisplayCaptureSize_256x64	= 1,
	DisplayCaptureSize_256x128	= 2,
	DisplayCaptureSize_256x192	= 3,
};

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
    u32 value;
	
	struct
	{
#ifdef LOCAL_LE
		u8 BG_Mode:3;						//  0- 2: A+B;
		u8 BG0_3D:1;						//     3: A  ; 0=2D,         1=3D
		u8 OBJ_Tile_mapping:1;				//     4: A+B; 0=2D (32KB),  1=1D (32..256KB)
		u8 OBJ_BMP_2D_dim:1;				//     5: A+B; 0=128x512,    1=256x256 pixels
		u8 OBJ_BMP_mapping:1;				//     6: A+B; 0=2D (128KB), 1=1D (128..256KB)
		u8 ForceBlank:1;					//     7: A+B;
		
		u8 BG0_Enable:1;					//     8: A+B; 0=Disable, 1=Enable
		u8 BG1_Enable:1;					//     9: A+B; 0=Disable, 1=Enable
		u8 BG2_Enable:1;					//    10: A+B; 0=Disable, 1=Enable
		u8 BG3_Enable:1;					//    11: A+B; 0=Disable, 1=Enable
		u8 OBJ_Enable:1;					//    12: A+B; 0=Disable, 1=Enable
		u8 Win0_Enable:1;					//    13: A+B; 0=Disable, 1=Enable
		u8 Win1_Enable:1;					//    14: A+B; 0=Disable, 1=Enable
		u8 WinOBJ_Enable:1;					//    15: A+B; 0=Disable, 1=Enable
		
		u8 DisplayMode:2;					// 16-17: A+B; coreA(0..3) coreB(0..1) GBA(Green Swap)
											//        0=off (white screen)
											//        1=on (normal BG & OBJ layers)
											//        2=VRAM display (coreA only)
											//        3=RAM display (coreA only, DMA transfers)
		u8 VRAM_Block:2;					// 18-19: A  ; VRAM block (0..3=A..D)
		u8 OBJ_Tile_1D_Bound:2;				//    20: A+B;
		u8 OBJ_BMP_1D_Bound:1;				// 21-22: A  ;
		u8 OBJ_HBlank_process:1;			//    23: A+B; OBJ processed during HBlank (GBA bit5)
		
		u8 CharacBase_Block:3;				// 24-26: A  ; Character Base (64K step)
		u8 ScreenBase_Block:3;				// 27-29: A  ; Screen Base (64K step)
		u8 ExBGxPalette_Enable:1;			//    30: A+B; 0=Disable, 1=Enable BG extended Palette
		u8 ExOBJPalette_Enable:1;			//    31: A+B; 0=Disable, 1=Enable OBJ extended Palette
#else
		u8 ForceBlank:1;					//     7: A+B;
		u8 OBJ_BMP_mapping:1;				//     6: A+B; 0=2D (128KB), 1=1D (128..256KB)
		u8 OBJ_BMP_2D_dim:1;				//     5: A+B; 0=128x512,    1=256x256 pixels
		u8 OBJ_Tile_mapping:1;				//     4: A+B; 0=2D (32KB),  1=1D (32..256KB)
		u8 BG0_3D:1;						//     3: A  ; 0=2D,         1=3D
		u8 BG_Mode:3;						//  0- 2: A+B;
		
		u8 WinOBJ_Enable:1;					//    15: A+B; 0=Disable, 1=Enable
		u8 Win1_Enable:1;					//    14: A+B; 0=Disable, 1=Enable
		u8 Win0_Enable:1;					//    13: A+B; 0=Disable, 1=Enable
		u8 OBJ_Enable:1;					//    12: A+B; 0=Disable, 1=Enable
		u8 BG3_Enable:1;					//    11: A+B; 0=Disable, 1=Enable
		u8 BG2_Enable:1;					//    10: A+B; 0=Disable, 1=Enable
		u8 BG1_Enable:1;					//     9: A+B; 0=Disable, 1=Enable
		u8 BG0_Enable:1;					//     8: A+B; 0=Disable, 1=Enable
		
		u8 OBJ_HBlank_process:1;			//    23: A+B; OBJ processed during HBlank (GBA bit5)
		u8 OBJ_BMP_1D_Bound:1;				//    22: A  ;
		u8 OBJ_Tile_1D_Bound:2;				// 20-21: A+B;
		u8 VRAM_Block:2;					// 18-19: A  ; VRAM block (0..3=A..D)
		u8 DisplayMode:2;					// 16-17: A+B; coreA(0..3) coreB(0..1) GBA(Green Swap)
											//        0=off (white screen)
											//        1=on (normal BG & OBJ layers)
											//        2=VRAM display (coreA only)
											//        3=RAM display (coreA only, DMA transfers)
		
		u8 ExOBJPalette_Enable:1;			//    31: A+B; 0=Disable, 1=Enable OBJ extended Palette
		u8 ExBGxPalette_Enable:1;			//    30: A+B; 0=Disable, 1=Enable BG extended Palette
		u8 ScreenBase_Block:3;				// 27-29: A  ; Screen Base (64K step)
		u8 CharacBase_Block:3;				// 24-26: A  ; Character Base (64K step)
#endif
	};
} IOREG_DISPCNT;							// 0x400x000: Display control (Engine A+B)

typedef union
{
	u16 value;
	
	struct
	{
		u16 VBlankFlag:1;					//     0: Set at V-Blank; 0=Not in V-Blank, 1=V-Blank occurred
		u16 HBlankFlag:1;					//     1: Set at H-Blank; 0=Not in H-Blank, 1=H-Blank occurred
		u16 VCounterFlag:1;					//     2: Set when this register's VCount matches the currently rendered scanline, interacts with VCOUNT (0x4000006); 0=Unmatched, 1=Matched
		u16 VBlankIRQ_Enable:1;				//     3: Send an IRQ when VBlankFlag is set; 0=Disable, 1=Enable
		u16 HBlankIRQ_Enable:1;				//     4: Send an IRQ when HBlankFlag is set; 0=Disable, 1=Enable
		u16 VCounterIRQ_Enable:1;			//     5: Send an IRQ when VCounterFlag is set; 0=Disable, 1=Enable
		u16 LCDInitReady:1;					//     6: Report that the LCD initialization is ready, DSi only; 0=Unready 1=Ready
		u16 VCount:9;						//  7-15: Current scanline, interacts with VCOUNT (0x4000006)
	};
} IOREG_DISPSTAT;							// 0x4000004: Display status (Engine A only)

typedef union
{
	u16 value;
	
	struct
	{
		u16 CurrentScanline:9;				//  0- 8: The scanline currently being rendered; 0...262
		u16 :7;								//  9-15: Unused bits
	};
} IOREG_VCOUNT;								// 0x4000006: Current display scanline (Engine A only)

/*******************************************************************************
    this structure is for display control of a specific layer,
    there are 4 background layers
    their priority indicate which one to draw on top of the other
    some flags indicate special drawing mode, size, FX
*******************************************************************************/
typedef union
{
    u16 value;
	
	struct
	{
#ifdef LOCAL_LE
		u8 Priority:2;						//  0- 1: Rendering priority; 0...3, where 0 is highest priority and 3 is lowest priority
		u8 CharacBase_Block:4;				//  2- 5: individual character base offset (n*16KB)
		u8 Mosaic:1;						//     6: Mosaic render: 0=Disable, 1=Enable
		u8 PaletteMode:1;					//     7: Color/palette mode; 0=16 palettes of 16 colors each, 1=Single palette of 256 colors
		
		u8 ScreenBase_Block:5;				//  8-12: individual screen base offset (text n*2KB, BMP n*16KB)
		u8 PaletteSet_Wrap:1;				//    13: BG0 extended palette set 0=set0, 1=set2
											//        BG1 extended palette set 0=set1, 1=set3
											//        BG2 overflow area wraparound 0=off, 1=wrap
											//        BG3 overflow area wraparound 0=off, 1=wrap
		u8 ScreenSize:2;					// 14-15: text    : 256x256 512x256 256x512 512x512
											//        x/rot/s : 128x128 256x256 512x512 1024x1024
											//        bmp     : 128x128 256x256 512x256 512x512
											//        large   : 512x1024 1024x512 - -
#else
		u8 PaletteMode:1;					//     7: Color/palette mode; 0=16 palettes of 16 colors each, 1=Single palette of 256 colors
		u8 Mosaic:1;						//     6: Mosaic render: 0=Disable, 1=Enable
		u8 CharacBase_Block:4;				//  2- 5: individual character base offset (n*16KB)
		u8 Priority:2;						//  0- 1: Rendering priority; 0...3, where 0 is highest priority and 3 is lowest priority
		
		u8 ScreenSize:2;					// 14-15: text    : 256x256 512x256 256x512 512x512
											//        x/rot/s : 128x128 256x256 512x512 1024x1024
											//        bmp     : 128x128 256x256 512x256 512x512
											//        large   : 512x1024 1024x512 - -
		u8 PaletteSet_Wrap:1;				//    13: BG0 extended palette set 0=set0, 1=set2
											//        BG1 extended palette set 0=set1, 1=set3
											//        BG2 overflow area wraparound 0=off, 1=wrap
											//        BG3 overflow area wraparound 0=off, 1=wrap
		u8 ScreenBase_Block:5;				//  8-12: individual screen base offset (text n*2KB, BMP n*16KB)
#endif
	};
} IOREG_BGnCNT;								// 0x400x008, 0x400x00A, 0x400x00C, 0x400x00E: BGn layer control (Engine A+B)

typedef IOREG_BGnCNT IOREG_BG0CNT;			// 0x400x008: BG0 layer control (Engine A+B)
typedef IOREG_BGnCNT IOREG_BG1CNT;			// 0x400x00A: BG1 layer control (Engine A+B)
typedef IOREG_BGnCNT IOREG_BG2CNT;			// 0x400x00C: BG2 layer control (Engine A+B)
typedef IOREG_BGnCNT IOREG_BG3CNT;			// 0x400x00E: BG3 layer control (Engine A+B)

typedef union
{
	u16 value;
	
	struct
	{
		u16 Offset:9;						//  0- 8: Offset value; 0...511
		u16 :7;								//  9-15: Unused bits
	};
} IOREG_BGnHOFS;							// 0x400x010, 0x400x014, 0x400x018, 0x400x01C: BGn horizontal offset (Engine A+B)

typedef IOREG_BGnHOFS IOREG_BGnVOFS;		// 0x400x012, 0x400x016, 0x400x01A, 0x400x01E: BGn vertical offset (Engine A+B)

typedef IOREG_BGnHOFS IOREG_BG0HOFS;		// 0x400x010: BG0 horizontal offset (Engine A+B)
typedef IOREG_BGnVOFS IOREG_BG0VOFS;		// 0x400x012: BG0 vertical offset (Engine A+B)
typedef IOREG_BGnHOFS IOREG_BG1HOFS;		// 0x400x014: BG1 horizontal offset (Engine A+B)
typedef IOREG_BGnVOFS IOREG_BG1VOFS;		// 0x400x016: BG1 vertical offset (Engine A+B)
typedef IOREG_BGnHOFS IOREG_BG2HOFS;		// 0x400x018: BG2 horizontal offset (Engine A+B)
typedef IOREG_BGnVOFS IOREG_BG2VOFS;		// 0x400x01A: BG2 vertical offset (Engine A+B)
typedef IOREG_BGnHOFS IOREG_BG3HOFS;		// 0x400x01C: BG3 horizontal offset (Engine A+B)
typedef IOREG_BGnVOFS IOREG_BG3VOFS;		// 0x400x01E: BG3 vertical offset (Engine A+B)

typedef struct
{
	IOREG_BGnHOFS BGnHOFS;
	IOREG_BGnVOFS BGnVOFS;
} IOREG_BGnOFS;								// 0x400x010, 0x400x014, 0x400x018, 0x400x01C: BGn horizontal offset (Engine A+B)

typedef union
{
	s16 value;
	
	struct
	{
		u16 Fraction:8;
		s16 Integer:8;
	};
} IOREG_BGnPA;								// 0x400x020, 0x400x030: BGn rotation/scaling parameter A (Engine A+B)
typedef IOREG_BGnPA IOREG_BGnPB;			// 0x400x022, 0x400x032: BGn rotation/scaling parameter B (Engine A+B)
typedef IOREG_BGnPA IOREG_BGnPC;			// 0x400x024, 0x400x034: BGn rotation/scaling parameter C (Engine A+B)
typedef IOREG_BGnPA IOREG_BGnPD;			// 0x400x026, 0x400x036: BGn rotation/scaling parameter D (Engine A+B)

typedef union
{
	u32 value;
	
	struct
	{
		u32 Fraction:8;
		s32 Integer:20;
		u32 :4;
	};
} IOREG_BGnX;								// 0x400x028, 0x400x038: BGn X-coordinate (Engine A+B)
typedef IOREG_BGnX IOREG_BGnY;				// 0x400x02C, 0x400x03C: BGn Y-coordinate (Engine A+B)

typedef IOREG_BGnPA IOREG_BG2PA;			// 0x400x020: BG2 rotation/scaling parameter A (Engine A+B)
typedef IOREG_BGnPB IOREG_BG2PB;			// 0x400x022: BG2 rotation/scaling parameter B (Engine A+B)
typedef IOREG_BGnPC IOREG_BG2PC;			// 0x400x024: BG2 rotation/scaling parameter C (Engine A+B)
typedef IOREG_BGnPD IOREG_BG2PD;			// 0x400x026: BG2 rotation/scaling parameter D (Engine A+B)
typedef IOREG_BGnX IOREG_BG2X;				// 0x400x028: BG2 X-coordinate (Engine A+B)
typedef IOREG_BGnY IOREG_BG2Y;				// 0x400x02C: BG2 Y-coordinate (Engine A+B)

typedef IOREG_BGnPA IOREG_BG3PA;			// 0x400x030: BG3 rotation/scaling parameter A (Engine A+B)
typedef IOREG_BGnPB IOREG_BG3PB;			// 0x400x032: BG3 rotation/scaling parameter B (Engine A+B)
typedef IOREG_BGnPC IOREG_BG3PC;			// 0x400x034: BG3 rotation/scaling parameter C (Engine A+B)
typedef IOREG_BGnPD IOREG_BG3PD;			// 0x400x036: BG3 rotation/scaling parameter D (Engine A+B)
typedef IOREG_BGnX IOREG_BG3X;				// 0x400x038: BG3 X-coordinate (Engine A+B)
typedef IOREG_BGnY IOREG_BG3Y;				// 0x400x03C: BG3 Y-coordinate (Engine A+B)

typedef struct
{
	IOREG_BGnPA BGnPA;						// 0x400x020, 0x400x030: BGn rotation/scaling parameter A (Engine A+B)
	IOREG_BGnPB BGnPB;						// 0x400x022, 0x400x032: BGn rotation/scaling parameter B (Engine A+B)
	IOREG_BGnPC BGnPC;						// 0x400x024, 0x400x034: BGn rotation/scaling parameter C (Engine A+B)
	IOREG_BGnPD BGnPD;						// 0x400x026, 0x400x036: BGn rotation/scaling parameter D (Engine A+B)
	IOREG_BGnX BGnX;						// 0x400x028, 0x400x038: BGn X-coordinate (Engine A+B)
	IOREG_BGnY BGnY;						// 0x400x02C, 0x400x03C: BGn Y-coordinate (Engine A+B)
} IOREG_BGnParameter;						// 0x400x020, 0x400x030: BGn parameters (Engine A+B)

typedef struct
{
	IOREG_BG2PA BG2PA;						// 0x400x020: BG2 rotation/scaling parameter A (Engine A+B)
	IOREG_BG2PB BG2PB;						// 0x400x022: BG2 rotation/scaling parameter B (Engine A+B)
	IOREG_BG2PC BG2PC;						// 0x400x024: BG2 rotation/scaling parameter C (Engine A+B)
	IOREG_BG2PD BG2PD;						// 0x400x026: BG2 rotation/scaling parameter D (Engine A+B)
	IOREG_BG2X BG2X;						// 0x400x028: BG2 X-coordinate (Engine A+B)
	IOREG_BG2Y BG2Y;						// 0x400x02C: BG2 Y-coordinate (Engine A+B)
} IOREG_BG2Parameter;						// 0x400x020: BG2 parameters (Engine A+B)

typedef struct
{
	IOREG_BG3PA BG3PA;						// 0x400x030: BG3 rotation/scaling parameter A (Engine A+B)
	IOREG_BG3PB BG3PB;						// 0x400x032: BG3 rotation/scaling parameter B (Engine A+B)
	IOREG_BG3PC BG3PC;						// 0x400x034: BG3 rotation/scaling parameter C (Engine A+B)
	IOREG_BG3PD BG3PD;						// 0x400x036: BG3 rotation/scaling parameter D (Engine A+B)
	IOREG_BG3X BG3X;						// 0x400x038: BG3 X-coordinate (Engine A+B)
	IOREG_BG3Y BG3Y;						// 0x400x03C: BG3 Y-coordinate (Engine A+B)
} IOREG_BG3Parameter;						// 0x400x030: BG3 parameters (Engine A+B)

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

typedef union
{
	u16 value;
	
	struct
	{
		u16 Right:8;						//  0- 7: Right coordinate of window
		u16 Left:8;							//  8-15: Left coordinate of window
	};
} IOREG_WIN0H;								// 0x400x040: Horizontal coordinates of Window 0 (Engine A+B)
typedef IOREG_WIN0H IOREG_WIN1H;			// 0x400x042: Horizontal coordinates of Window 1 (Engine A+B)

typedef union
{
	u16 value;
	
	struct
	{
		u16 Bottom:8;						//  0- 7: Bottom coordinate of window
		u16 Top:8;							//  8-15: Top coordinate of window
	};
} IOREG_WIN0V;								// 0x400x044: Vertical coordinates of Window 0 (Engine A+B)
typedef IOREG_WIN0V IOREG_WIN1V;			// 0x400x046: Vertical coordinates of Window 1 (Engine A+B)

typedef union
{
	u8 value;
	
	struct
	{
#ifdef LOCAL_LE
		u8 BG0_Enable:1;					//     0: Layer BG0 display; 0=Disable, 1=Enable
		u8 BG1_Enable:1;					//     1: Layer BG1 display; 0=Disable, 1=Enable
		u8 BG2_Enable:1;					//     2: Layer BG2 display; 0=Disable, 1=Enable
		u8 BG3_Enable:1;					//     3: Layer BG3 display; 0=Disable, 1=Enable
		u8 OBJ_Enable:1;					//     4: Layer OBJ display; 0=Disable, 1=Enable
		u8 Effect_Enable:1;					//     5: Color special effect; 0=Disable, 1=Enable
		u8 :2;								//  6- 7: Unused bits
#else
		u8 :2;								//  6- 7: Unused bits
		u8 Effect_Enable:1;					//     5: Color special effect; 0=Disable, 1=Enable
		u8 OBJ_Enable:1;					//     4: Layer OBJ display; 0=Disable, 1=Enable
		u8 BG3_Enable:1;					//     3: Layer BG3 display; 0=Disable, 1=Enable
		u8 BG2_Enable:1;					//     2: Layer BG2 display; 0=Disable, 1=Enable
		u8 BG1_Enable:1;					//     1: Layer BG1 display; 0=Disable, 1=Enable
		u8 BG0_Enable:1;					//     0: Layer BG0 display; 0=Disable, 1=Enable
#endif
	};
} IOREG_WIN0IN;								// 0x400x048: Control of inside of Window 0 (highest priority)
typedef IOREG_WIN0IN IOREG_WIN1IN;			// 0x400x049: Control of inside of Window 1 (medium priority)
typedef IOREG_WIN0IN IOREG_WINOUT;			// 0x400x04A: Control of outside of all windows
typedef IOREG_WIN0IN IOREG_WINOBJ;			// 0x400x04B: Control of inside of Window OBJ (lowest priority)

typedef union
{
	u32 value;
	
	struct
	{
#ifdef LOCAL_LE
		u32 BG_MosaicH:4;					//  0- 3: Mosaic pixel width for BG layers; 0...15
		u32 BG_MosaicV:4;					//  4- 7: Mosaic pixel height for BG layers; 0...15
		
		u32 OBJ_MosaicH:4;					//  8-11: Mosaic pixel width for OBJ layer; 0...15
		u32 OBJ_MosaicV:4;					// 12-15: Mosaic pixel height for OBJ layer; 0...15
#else
		u32 BG_MosaicV:4;					//  4- 7: Mosaic pixel height for BG layers; 0...15
		u32 BG_MosaicH:4;					//  0- 3: Mosaic pixel width for BG layers; 0...15
		
		u32 OBJ_MosaicV:4;					// 12-15: Mosaic pixel height for OBJ layer; 0...15
		u32 OBJ_MosaicH:4;					//  8-11: Mosaic pixel width for OBJ layer; 0...15
#endif
		
		u32 :16;							// 16-31: Unused bits
	};
} IOREG_MOSAIC;								// 0x400x04C: Mosaic size (Engine A+B)

typedef union
{
	u16 value;
	
	struct
	{
#ifdef LOCAL_LE
		u16 BG0_Target1:1;					//     0: Select layer BG0 for 1st target; 0=Disable, 1=Enable
		u16 BG1_Target1:1;					//     1: Select layer BG1 for 1st target; 0=Disable, 1=Enable
		u16 BG2_Target1:1;					//     2: Select layer BG2 for 1st target; 0=Disable, 1=Enable
		u16 BG3_Target1:1;					//     3: Select layer BG3 for 1st target; 0=Disable, 1=Enable
		u16 OBJ_Target1:1;					//     4: Select layer OBJ for 1st target; 0=Disable, 1=Enable
		u16 Backdrop_Target1:1;				//     5: Select backdrop for 1st target; 0=Disable, 1=Enable
		u16 ColorEffect:2;					//  6- 7: Color effect mode;
											//        0=Disable
											//        1=Alpha blend 1st and 2nd target, interacts with BLDALPHA (0x400x052)
											//        2=Increase brightness, interacts with BLDY (0x400x054)
											//        3=Decrease brightness, interacts with BLDY (0x400x054)
		
		u16 BG0_Target2:1;					//     8: Select layer BG0 for 2nd target; 0=Disable, 1=Enable
		u16 BG1_Target2:1;					//     9: Select layer BG1 for 2nd target; 0=Disable, 1=Enable
		u16 BG2_Target2:1;					//    10: Select layer BG2 for 2nd target; 0=Disable, 1=Enable
		u16 BG3_Target2:1;					//    11: Select layer BG3 for 2nd target; 0=Disable, 1=Enable
		u16 OBJ_Target2:1;					//    12: Select layer OBJ for 2nd target; 0=Disable, 1=Enable
		u16 Backdrop_Target2:1;				//    13: Select backdrop for 2nd target; 0=Disable, 1=Enable
		u16 :2;								// 14-15: Unused bits
#else
		u16 ColorEffect:2;					//  6- 7: Color effect mode;
											//        0=Disable
											//        1=Alpha blend 1st and 2nd target, interacts with BLDALPHA (0x400x052)
											//        2=Increase brightness, interacts with BLDY (0x400x054)
											//        3=Decrease brightness, interacts with BLDY (0x400x054)
		u16 Backdrop_Target1:1;				//     5: Select backdrop for 1st target; 0=Disable, 1=Enable
		u16 OBJ_Target1:1;					//     4: Select layer OBJ for 1st target; 0=Disable, 1=Enable
		u16 BG3_Target1:1;					//     3: Select layer BG3 for 1st target; 0=Disable, 1=Enable
		u16 BG2_Target1:1;					//     2: Select layer BG2 for 1st target; 0=Disable, 1=Enable
		u16 BG1_Target1:1;					//     1: Select layer BG1 for 1st target; 0=Disable, 1=Enable
		u16 BG0_Target1:1;					//     0: Select layer BG0 for 1st target; 0=Disable, 1=Enable
		
		u16 :2;								// 14-15: Unused bits
		u16 Backdrop_Target2:1;				//    13: Select backdrop for 2nd target; 0=Disable, 1=Enable
		u16 OBJ_Target2:1;					//    12: Select layer OBJ for 2nd target; 0=Disable, 1=Enable
		u16 BG3_Target2:1;					//    11: Select layer BG3 for 2nd target; 0=Disable, 1=Enable
		u16 BG2_Target2:1;					//    10: Select layer BG2 for 2nd target; 0=Disable, 1=Enable
		u16 BG1_Target2:1;					//     9: Select layer BG1 for 2nd target; 0=Disable, 1=Enable
		u16 BG0_Target2:1;					//     8: Select layer BG0 for 2nd target; 0=Disable, 1=Enable
#endif
	};
} IOREG_BLDCNT;								// 0x400x050: Color effects selection (Engine A+B)

typedef union
{
	u16 value;
	
	struct
	{
#ifdef LOCAL_LE
		u16 EVA:5;							//  0- 4: Blending coefficient for 1st target; 0...31 (clamped to 16)
		u16 :3;								//  5- 7: Unused bits
		
		u16 EVB:5;							//  8-12: Blending coefficient for 2nd target; 0...31 (clamped to 16)
		u16 :3;								// 13-15: Unused bits
#else
		u16 :3;								//  5- 7: Unused bits
		u16 EVA:5;							//  0- 4: Blending coefficient for 1st target; 0...31 (clamped to 16)
		
		u16 :3;								// 13-15: Unused bits
		u16 EVB:5;							//  8-12: Blending coefficient for 2nd target; 0...31 (clamped to 16)
#endif
	};
} IOREG_BLDALPHA;							// 0x400x052: Color effects selection, interacts with BLDCNT (0x400x050) (Engine A+B)

typedef union
{
	u16 value;
	
	struct
	{
#ifdef LOCAL_LE
		u16 EVY:5;							//  0- 4: Blending coefficient for increase/decrease brightness; 0...31 (clamped to 16)
		u16 :3;								//  5- 7: Unused bits
#else
		u16 :3;								//  5- 7: Unused bits
		u16 EVY:5;							//  0- 4: Blending coefficient for increase/decrease brightness; 0...31 (clamped to 16)
#endif
		u16 :8;								//  8-15: Unused bits
	};
} IOREG_BLDY;								// 0x400x054: Color effects selection, interacts with BLDCNT (0x400x050) (Engine A+B)

typedef union
{
    u32 value;
	
	struct
	{
#ifdef LOCAL_LE
		u8 EnableTexMapping:1;				//     0: Apply textures; 0=Disable, 1=Enable
		u8 PolygonShading:1;				//     1: Polygon shading mode, interacts with POLYGON_ATTR (0x40004A4); 0=Toon Shading, 1=Highlight Shading
		u8 EnableAlphaTest:1;				//     2: Perform alpha test, interacts with ALPHA_TEST_REF (0x4000340); 0=Disable, 1=Enable
		u8 EnableAlphaBlending:1;			//     3: Perform alpha blending, interacts with POLYGON_ATTR (0x40004A4); 0=Disable, 1=Enable
		u8 EnableAntiAliasing:1;			//     4: Render polygon edges with antialiasing; 0=Disable, 1=Enable
		u8 EnableEdgeMarking:1;				//     5: Perform polygon edge marking, interacts with EDGE_COLOR (0x4000330); 0=Disable, 1=Enable
		u8 FogOnlyAlpha:1;					//     6: Apply fog to the alpha channel only, interacts with FOG_COLOR (0x4000358) / FOG_TABLE (0x4000360); 0=Color+Alpha, 1=Alpha
		u8 EnableFog:1;						//     7: Perform fog rendering, interacts with FOG_COLOR (0x4000358) / FOG_OFFSET (0x400035C) / FOG_TABLE (0x4000360);
											//        0=Disable, 1=Enable
		
		u8 FogShiftSHR:4;					//  8-11: SHR-Divider, interacts with FOG_OFFSET (0x400035C); 0...10
		u8 AckColorBufferUnderflow:1;		//    12: Color Buffer RDLINES Underflow; 0=None, 1=Underflow/Acknowledge
		u8 AckVertexRAMOverflow:1;			//    13: Polygon/Vertex RAM Overflow; 0=None, 1=Overflow/Acknowledge
		u8 RearPlaneMode:1;					//    14: Use clear image, interacts with CLEAR_COLOR (0x4000350) / CLEAR_DEPTH (0x4000354) / CLRIMAGE_OFFSET (0x4000356);
											//        0=Blank, 1=Bitmap
		u8 :1;								//    15: Unused bits
		
		u16 :16;							// 16-31: Unused bits
#else
		u8 EnableFog:1;						//     7: Perform fog rendering, interacts with FOG_COLOR (0x4000358) / FOG_OFFSET (0x400035C) / FOG_TABLE (0x4000360);
											//        0=Disable, 1=Enable
		u8 FogOnlyAlpha:1;					//     6: Apply fog to the alpha channel only, interacts with FOG_COLOR (0x4000358) / FOG_TABLE (0x4000360); 0=Color+Alpha, 1=Alpha
		u8 EnableEdgeMarking:1;				//     5: Perform polygon edge marking, interacts with EDGE_COLOR (0x4000330); 0=Disable, 1=Enable
		u8 EnableAntiAliasing:1;			//     4: Render polygon edges with antialiasing; 0=Disable, 1=Enable
		u8 EnableAlphaBlending:1;			//     3: Perform alpha blending, interacts with POLYGON_ATTR (0x40004A4); 0=Disable, 1=Enable
		u8 EnableAlphaTest:1;				//     2: Perform alpha test, interacts with ALPHA_TEST_REF (0x4000340); 0=Disable, 1=Enable
		u8 PolygonShading:1;				//     1: Polygon shading mode, interacts with POLYGON_ATTR (0x40004A4); 0=Toon Shading, 1=Highlight Shading
		u8 EnableTexMapping:1;				//     0: Apply textures; 0=Disable, 1=Enable
		
		u8 :1;								//    15: Unused bits
		u8 RearPlaneMode:1;					//    14: Use clear image, interacts with CLEAR_COLOR (0x4000350) / CLEAR_DEPTH (0x4000354) / CLRIMAGE_OFFSET (0x4000356);
											//        0=Blank, 1=Bitmap
		u8 AckVertexRAMOverflow:1;			//    13: Polygon/Vertex RAM Overflow; 0=None, 1=Overflow/Acknowledge
		u8 AckColorBufferUnderflow:1;		//    12: Color Buffer RDLINES Underflow; 0=None, 1=Underflow/Acknowledge
		u8 FogShiftSHR:4;					//  8-11: SHR-Divider, interacts with FOG_OFFSET (0x400035C); 0...10
		
		u16 :16;							// 16-31: Unused bits
#endif
	};
} IOREG_DISP3DCNT;							// 0x4000060: 3D engine control (Engine A only)

typedef union
{
	u32 value;
	
	struct
	{
#ifdef LOCAL_LE
		unsigned EVA:5;						//  0- 4: Blending coefficient for SrcA; 0...31 (clamped to 16)
		unsigned :3;						//  5- 7: Unused bits
		
		unsigned EVB:5;						//  8-12: Blending coefficient for SrcB; 0...31 (clamped to 16)
		unsigned :3;						// 13-15: Unused bits
		
		unsigned VRAMWriteBlock:2;			// 16-17: VRAM write target block; 0=Block A, 1=Block B, 2=Block C, 3=Block D
		unsigned VRAMWriteOffset:2;			// 18-19: VRAM write target offset; 0=0KB, 1=32KB, 2=64KB, 3=96KB
		unsigned CaptureSize:2;				// 20-21: Display capture dimensions; 0=128x128, 1=256x64, 2=256x128, 3=256x192
		unsigned :2;						// 22-23: Unused bits
		
		unsigned SrcA:1;					//    24: SrcA target; 0=Current framebuffer, 1=3D render buffer
		unsigned SrcB:1;					//    25: SrcB target;
											//        0=VRAM block, interacts with DISPCNT (0x4000000)
											//        1=Main memory FIFO, interacts with DISP_MMEM_FIFO (0x4000068)
		unsigned VRAMReadOffset:2;			// 26-27: VRAM read target offset; 0=0KB, 1=32KB, 2=64KB, 3=96KB
		unsigned :1;						//    28: Unused bit
		unsigned CaptureSrc:2;				// 29-30: Select capture target; 0=SrcA, 1=SrcB, 2=SrcA+SrcB blend, 3=SrcA+SrcB blend
		unsigned CaptureEnable:1;			//    31: Display capture status; 0=Disable/Ready 1=Enable/Busy
#else
		unsigned :3;						//  5- 7: Unused bits
		unsigned EVA:5;						//  0- 4: Blending coefficient for SrcA; 0...31 (clamped to 16)
		
		unsigned :3;						// 13-15: Unused bits
		unsigned EVB:5;						//  8-12: Blending coefficient for SrcB; 0...31 (clamped to 16)
		
		unsigned :2;						// 22-23: Unused bits
		unsigned CaptureSize:2;				// 20-21: Display capture dimensions; 0=128x128, 1=256x64, 2=256x128, 3=256x192
		unsigned VRAMWriteOffset:2;			// 18-19: VRAM write target offset; 0=0KB, 1=32KB, 2=64KB, 3=96KB
		unsigned VRAMWriteBlock:2;			// 16-17: VRAM write target block; 0=Block A, 1=Block B, 2=Block C, 3=Block D
		
		unsigned CaptureEnable:1;			//    31: Display capture status; 0=Disable/Ready 1=Enable/Busy
		unsigned CaptureSrc:2;				// 29-30: Select capture target; 0=SrcA, 1=SrcB, 2=SrcA+SrcB blend, 3=SrcA+SrcB blend
		unsigned :1;						//    28: Unused bit
		unsigned VRAMReadOffset:2;			// 26-27: VRAM read target offset; 0=0KB, 1=32KB, 2=64KB, 3=96KB
		unsigned SrcB:1;					//    25: SrcB target;
											//        0=VRAM block, interacts with DISPCNT (0x4000000)
											//        1=Main memory FIFO, interacts with DISP_MMEM_FIFO (0x4000068)
		unsigned SrcA:1;					//    24: SrcA target; 0=Current framebuffer, 1=3D render buffer
#endif
	};
	
} IOREG_DISPCAPCNT;							// 0x4000064: Display capture control (Engine A only)

typedef u32 IOREG_DISP_MMEM_FIFO;			// 0x4000068: Main memory FIFO data (Engine A only)

typedef union
{
	u32 value;
	
	struct
	{
#ifdef LOCAL_LE
		u32 Intensity:5;					//  0- 4: Brightness coefficient for increase/decrease brightness; 0...31 (clamped to 16)
		u32 :3;								//  5- 7: Unused bits
		
		u32 :6;								//  8-13: Unused bits
		u32 Mode:2;							// 14-15: Brightness mode; 0=Disable, 1=Increase, 2=Decrease, 3=Reserved
		
		u32 :16;							// 16-31: Unused bits
#else
		u32 :3;								//  5- 7: Unused bits
		u32 Intensity:5;					//  0- 4: Brightness coefficient for increase/decrease brightness; 0...31 (clamped to 16)
		
		u32 Mode:2;							// 14-15: Brightness mode; 0=Disable, 1=Increase, 2=Decrease, 3=Reserved
		u32 :6;								//  8-13: Unused bits
		
		u32 :16;							// 16-31: Unused bits
#endif
	};
} IOREG_MASTER_BRIGHT;						// 0x400x06C: Master brightness effect (Engine A+B)

#include "PACKED.h"
typedef struct
{
	IOREG_DISPCNT			DISPCNT;				// 0x0400x000
	IOREG_DISPSTAT			DISPSTAT;				// 0x04000004
	IOREG_VCOUNT			VCOUNT;					// 0x04000006
	
	union
	{
		IOREG_BGnCNT		BGnCNT[4];				// 0x0400x008
		
		struct
		{
			IOREG_BG0CNT	BG0CNT;					// 0x0400x008
			IOREG_BG1CNT	BG1CNT;					// 0x0400x00A
			IOREG_BG2CNT	BG2CNT;					// 0x0400x00C
			IOREG_BG3CNT	BG3CNT;					// 0x0400x00E
		};
	};
	
	union
	{
		IOREG_BGnOFS		BGnOFS[4];				// 0x0400x010
		
		struct
		{
			IOREG_BG0HOFS	BG0HOFS;				// 0x0400x010
			IOREG_BG0VOFS	BG0VOFS;				// 0x0400x012
			IOREG_BG1HOFS	BG1HOFS;				// 0x0400x014
			IOREG_BG1VOFS	BG1VOFS;				// 0x0400x016
			IOREG_BG2HOFS	BG2HOFS;				// 0x0400x018
			IOREG_BG2VOFS	BG2VOFS;				// 0x0400x01A
			IOREG_BG3HOFS	BG3HOFS;				// 0x0400x01C
			IOREG_BG3VOFS	BG3VOFS;				// 0x0400x01E
		};
	};
	
	union
	{
		IOREG_BGnParameter			BGnParam[2];	// 0x0400x020
		
		struct
		{
			union
			{
				IOREG_BG2Parameter	BG2Param;		// 0x0400x020
				
				struct
				{
					IOREG_BG2PA		BG2PA;			// 0x0400x020
					IOREG_BG2PB		BG2PB;			// 0x0400x022
					IOREG_BG2PC		BG2PC;			// 0x0400x024
					IOREG_BG2PD		BG2PD;			// 0x0400x026
					IOREG_BG2X		BG2X;			// 0x0400x028
					IOREG_BG2Y		BG2Y;			// 0x0400x02C
				};
			};
			
			union
			{
				IOREG_BG3Parameter	BG3Param;		// 0x0400x030
				
				struct
				{
					IOREG_BG3PA		BG3PA;			// 0x0400x030
					IOREG_BG3PB		BG3PB;			// 0x0400x032
					IOREG_BG3PC		BG3PC;			// 0x0400x034
					IOREG_BG3PD		BG3PD;			// 0x0400x036
					IOREG_BG3X		BG3X;			// 0x0400x038
					IOREG_BG3Y		BG3Y;			// 0x0400x03C
				};
			};
		};
	};
	
	IOREG_WIN0H				WIN0H;					// 0x0400x040
	IOREG_WIN1H				WIN1H;					// 0x0400x042
	IOREG_WIN0V				WIN0V;					// 0x0400x044
	IOREG_WIN1V				WIN1V;					// 0x0400x046
	IOREG_WIN0IN			WIN0IN;					// 0x0400x048
	IOREG_WIN1IN			WIN1IN;					// 0x0400x049
	IOREG_WINOUT			WINOUT;					// 0x0400x04A
	IOREG_WINOBJ			WINOBJ;					// 0x0400x04B
	
	IOREG_MOSAIC			MOSAIC;					// 0x0400x04C
	
	IOREG_BLDCNT			BLDCNT;					// 0x0400x050
	IOREG_BLDALPHA			BLDALPHA;				// 0x0400x052
	IOREG_BLDY				BLDY;					// 0x0400x054
	
	u8						unused[10];				// 0x0400x056
	
	IOREG_DISP3DCNT			DISP3DCNT;				// 0x04000060
	IOREG_DISPCAPCNT		DISPCAPCNT;				// 0x04000064
	IOREG_DISP_MMEM_FIFO	DISP_MMEM_FIFO;			// 0x04000068
	
	IOREG_MASTER_BRIGHT		MASTER_BRIGHT;			// 0x0400x06C
} GPU_IOREG;										// 0x04000000, 0x04001000: GPU registers
#include "PACKED_END.h"

enum ColorEffect
{
	ColorEffect_Disable					= 0,
	ColorEffect_Blend					= 1,
	ColorEffect_IncreaseBrightness		= 2,
	ColorEffect_DecreaseBrightness		= 3
};

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

#define COLOR_16_32(w,i)	\
	/* doesnt matter who's 16bit who's 32bit */ \
	i.bits.red   = w.bits.red; \
	i.bits.green = w.bits.green; \
	i.bits.blue  = w.bits.blue; \
	i.bits.alpha = w.bits.alpha;

typedef union
{
	u16 attr[4];
	
	struct
	{
#ifdef LOCAL_LE
		union
		{
			u16 attr0;
			
			struct
			{
				u16 Y:8;					//  0- 7: Sprite Y-coordinate; 0...255
				u16 RotScale:1;				//     8: Perform rotation/scaling; 0=Disable, 1=Enable
				u16 Disable:1;				//     9: OBJ disable flag, only if Bit8 is cleared; 0=Perform render, 1=Do not perform render
				u16 Mode:2;					// 10-11: OBJ mode; 0=Normal, 1=Transparent, 2=Window, 3=Bitmap
				u16 Mosaic:1;				//    12: Mosaic render: 0=Disable, 1=Enable
				u16 PaletteMode:1;			//    13: Color/palette select; 0=16 palettes of 16 colors each, 1=Single palette of 256 colors
				u16 Shape:2;				// 14-15: OBJ shape; 0=Square, 1=Horizontal, 2=Vertical, 3=Prohibited
			};
			
			struct
			{
				u16 :8;
				u16 :1;
				u16 DoubleSize:1;			//     9: Perform double-size render, only if Bit8 is set; 0=Disable, 1=Enable
				u16 :6;
			};
		};
		
		s16 X:9;							// 16-24: Sprite X-coordinate; 0...511
		u16 RotScaleIndex:3;				// 25-27: Rotation/scaling parameter selection; 0...31
		u16 HFlip:1;						//    28: Flip sprite horizontally; 0=Normal, 1=Flip
		u16 VFlip:1;						//    29: Flip sprite vertically; 0=Normal, 1=Flip
		u16 Size:2;							// 30-31: OBJ size, interacts with Bit 14-15
											//
											//        Size| Square | Horizontal | Vertical
											//           0:   8x8       16x8        8x16
											//           1:  16x16      32x8        8x32
											//           2:  32x32      32x16      16x32
											//           3:  64x64      64x32      32x64
		u16 TileIndex:10;					// 32-41: Tile index; 0...1023
		
		u16 Priority:2;						// 42-43: Rendering priority; 0...3, where 0 is highest priority and 3 is lowest priority
		u16 PaletteIndex:4;					// 44-47: Palette index; 0...15
#else
		union
		{
			u16 attr0;
			
			struct
			{
				u16 Y:8;					//  0- 7: Sprite Y-coordinate; 0...255
				u16 Shape:2;				// 14-15: OBJ shape; 0=Square, 1=Horizontal, 2=Vertical, 3=Prohibited
				u16 PaletteMode:1;			//    13: Color/palette select; 0=16 palettes of 16 colors each, 1=Single palette of 256 colors
				u16 Mosaic:1;				//    12: Mosaic render: 0=Disable, 1=Enable
				u16 Mode:2;					// 10-11: OBJ mode; 0=Normal, 1=Transparent, 2=Window, 3=Bitmap
				u16 Disable:1;				//     9: OBJ disable flag, only if Bit8 is cleared; 0=Perform render, 1=Do not perform render
				u16 RotScale:1;				//     8: Perform rotation/scaling; 0=Disable, 1=Enable
			};
			
			struct
			{
				u16 :8;
				u16 :6;
				u16 DoubleSize:1;			//     9: Perform double-size render, only if Bit8 is set; 0=Disable, 1=Enable
				u16 :1;
			};
		};
		
		// 16-31: Whenever this is used, you will need to explicitly convert endianness.
		u16 Size:2;							// 30-31: OBJ size, interacts with Bit 14-15
											//
											//        Size| Square | Horizontal | Vertical
											//           0:   8x8       16x8        8x16
											//           1:  16x16      32x8        8x32
											//           2:  32x32      32x16      16x32
											//           3:  64x64      64x32      32x64
		u16 VFlip:1;						//    29: Flip sprite vertically; 0=Normal, 1=Flip
		u16 HFlip:1;						//    28: Flip sprite horizontally; 0=Normal, 1=Flip
		u16 RotScaleIndex:3;				// 25-27: Rotation/scaling parameter selection; 0...31
		s16 X:9;							// 16-24: Sprite X-coordinate; 0...511
		
		// 32-47: Whenever this is used, you will need to explicitly convert endianness.
		u16 PaletteIndex:4;					// 44-47: Palette index; 0...15
		u16 Priority:2;						// 42-43: Rendering priority; 0...3, where 0 is highest priority and 3 is lowest priority
		u16 TileIndex:10;					// 32-41: Tile index; 0...1023
#endif
		
		u16 attr3:16;						// 48-63: Whenever this is used, you will need to explicitly convert endianness.
	};
} OAMAttributes;

enum SpriteRenderMode
{
	SpriteRenderMode_Sprite1D = 0,
	SpriteRenderMode_Sprite2D = 1
};

typedef struct
{
	u16 width;
	u16 height;
} GPUSize_u16;

typedef GPUSize_u16 SpriteSize;
typedef GPUSize_u16 BGLayerSize;

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
	GPULayerID_Backdrop				= 5
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

struct DISPCAPCNT_parsed
{
	u8 EVA;
	u8 EVB;
	u8 readOffset;
	u16 capy;
};

typedef struct
{
	// User-requested settings. These fields will always remain constant, and can only be changed if
	// the user calls GPUSubsystem::SetFramebufferSize().
	
	bool isCustomSizeRequested;			// Reports that the call to GPUSubsystem::SetFramebufferSize() resulted in a custom rendering size.
										//    true - The user requested a custom size.
										//    false - The user requested the native size.
	size_t customWidth;					// The requested custom width, measured in pixels.
	size_t customHeight;				// The requested custom height, measured in pixels.
	
	u16 *masterNativeBuffer;			// Pointer to the head of the master native buffer.
	u16 *masterCustomBuffer;			// Pointer to the head of the master custom buffer.
										// If GPUSubsystem::GetWillAutoResolveToCustomBuffer() would return true, or if
										// GPUEngineBase::ResolveToCustomFramebuffer() is called, then this buffer is used as the target
										// buffer for resolving any native-sized renders.
	
	
	// Frame information. These fields will change per frame, depending on how each display was rendered.
	
	u16	*nativeBuffer[2];				// Pointer to the display's native size framebuffer.
	u16 *customBuffer[2];				// Pointer to the display's custom size framebuffer.
	
	bool didPerformCustomRender[2];		// Reports that the display actually rendered at a custom size for this frame.
										//    true - The display performed a custom-sized render.
										//    false - The display performed a native-sized render.
	size_t renderedWidth[2];			// The display rendered at this width, measured in pixels.
	size_t renderedHeight[2];			// The display rendered at this height, measured in pixels.
	u16 *renderedBuffer[2];				// The display rendered to this buffer.
} NDSDisplayInfo;

#define VRAM_NO_3D_USAGE 0xFF

typedef struct
{
	GPULayerID layerID;
	IOREG_BGnCNT BGnCNT;
	IOREG_BGnHOFS BGnHOFS;
	IOREG_BGnVOFS BGnVOFS;
	
	BGLayerSize size;
	BGType baseType;
	BGType type;
	u8 priority;
	
	bool isVisible;
	bool isMosaic;
	bool isDisplayWrapped;
	
	u8 extPaletteSlot;
	u16 **extPalette;
	
	u32 largeBMPAddress;
	u32 BMPAddress;
	u32 tileMapAddress;
	u32 tileEntryAddress;
	
	u16 xOffset;
	u16 yOffset;
} BGLayerInfo;

class GPUEngineBase
{
protected:
	static CACHE_ALIGN u16 _fadeInColors[17][0x8000];
	static CACHE_ALIGN u16 _fadeOutColors[17][0x8000];
	static CACHE_ALIGN u8 _blendTable555[17][17][32][32];
	
	static const CACHE_ALIGN SpriteSize _sprSizeTab[4][4];
	static const CACHE_ALIGN BGLayerSize _BGLayerSizeLUT[8][4];
	static const CACHE_ALIGN BGType _mode2type[8][4];
	static const CACHE_ALIGN u8 _winEmpty[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	
	static struct MosaicLookup {
		
		struct TableEntry {
			u8 begin;
			u8 trunc;
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
	} _mosaicLookup;
	
	CACHE_ALIGN u16 _sprColor[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _sprAlpha[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _sprType[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _sprPrio[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _sprWin[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	
	CACHE_ALIGN u8 _bgLayerIndex[GPU_FRAMEBUFFER_NATIVE_WIDTH * 4];
	CACHE_ALIGN u16 _bgLayerColor[GPU_FRAMEBUFFER_NATIVE_WIDTH * 4];
	
	u8 *_bgLayerIndexCustom;
	u16 *_bgLayerColorCustom;
	
	bool _enableLayer[5];
	bool _isAnyBGLayerEnabled;
	itemsForPriority_t _itemsForPriority[NB_PRIORITIES];
	GPUDisplayMode _displayOutputMode;
	
	struct MosaicColor {
		u16 bg[4][256];
		struct Obj {
			u16 color;
			u8 alpha;
			u8 opaque;
		} obj[256];
	} _mosaicColors;
	
	GPUEngineID _engineID;
	GPU_IOREG *_IORegisterMap;
	u16 *_paletteBG;
	u16 *_paletteOBJ;
	OAMAttributes *_oamList;
	u32 _sprMem;
	
	u8 _sprBoundary;
	u8 _sprBMPBoundary;
	
	bool _blend2[6];
#ifdef ENABLE_SSSE3
	__m128i _blend2_SSSE3;
#endif
	
	TBlendTable *_blendTable;
	u16 *_currentFadeInColors;
	u16 *_currentFadeOutColors;
	
	BGLayerInfo _BGLayer[4];
	
	CACHE_ALIGN u8 _sprNum[256];
	CACHE_ALIGN u8 _h_win[2][GPU_FRAMEBUFFER_NATIVE_WIDTH];
	const u8 *_curr_win[2];
	
	NDSDisplayID _targetDisplayID;
	SpriteRenderMode _spriteRenderMode;
	bool _isMasterBrightFullIntensity;
	
	CACHE_ALIGN u16 _internalRenderLineTargetNative[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _renderLineLayerIDNative[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	
	u16 *_internalRenderLineTargetCustom;
	u8 *_renderLineLayerIDCustom;
	bool _needUpdateWINH[2];
	
	bool _WIN0_ENABLED;
	bool _WIN1_ENABLED;
	bool _WINOBJ_ENABLED;
	bool _isAnyWindowEnabled;
	
	MosaicLookup::TableEntry *_mosaicWidthBG;
	MosaicLookup::TableEntry *_mosaicHeightBG;
	MosaicLookup::TableEntry *_mosaicWidthOBJ;
	MosaicLookup::TableEntry *_mosaicHeightOBJ;
	bool _isBGMosaicSet;
	bool _isOBJMosaicSet;
	
	u8 _BLDALPHA_EVA;
	u8 _BLDALPHA_EVB;
	u8 _BLDALPHA_EVY;
	
	void _InitLUTs();
	void _Reset_Base();
	void _ResortBGLayers();
	
	template <bool NATIVEDST, bool NATIVESRC, bool USELINEINDEX, bool NEEDENDIANSWAP> void _LineColorCopy(u16 *__restrict dstBuffer, const u16 *__restrict srcBuffer, const size_t l);
	template <bool NATIVEDST, bool NATIVESRC> void _LineLayerIDCopy(u8 *__restrict dstBuffer, const u8 *__restrict srcBuffer, const size_t l);
	
	void _MosaicSpriteLinePixel(const size_t x, u16 l, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab);
	void _MosaicSpriteLine(u16 l, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab);
	
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED, PixelLookupFunc GetPixelFunc, bool WRAP> void _RenderPixelIterate_Final(u16 *__restrict dstColorLine, const u16 lineIndex, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal);
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED, PixelLookupFunc GetPixelFunc, bool WRAP> void _RenderPixelIterate_ApplyWrap(u16 *__restrict dstColorLine, const u16 lineIndex, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal);
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED, PixelLookupFunc GetPixelFunc> void _RenderPixelIterate(u16 *__restrict dstColorLine, const u16 lineIndex, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal);
	
	TILEENTRY _GetTileEntry(const u32 tileMapAddress, const u16 xOffset, const u16 layerWidthMask);
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT> FORCEINLINE void _RenderPixelSingle(u16 *__restrict dstColorLine, u8 *__restrict dstLayerID, const size_t lineIndex, u16 color, const size_t srcX, const bool opaque);
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT> void _RenderPixelsCustom(u16 *__restrict dstColorLine, u8 *__restrict dstLayerID, const size_t lineIndex);
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT> void _RenderPixelsCustomVRAM(u16 *__restrict dstColorLine, u8 *__restrict dstLayerID, const size_t lineIndex);
	
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED> void _RenderLine_BGText(u16 *__restrict dstColorLine, const u16 lineIndex, const u16 XBG, const u16 YBG);
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED> void _RenderLine_BGAffine(u16 *__restrict dstColorLine, const u16 lineIndex, const IOREG_BGnParameter &param);
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED> u16* _RenderLine_BGExtended(u16 *__restrict dstColorLine, const u16 lineIndex, const IOREG_BGnParameter &param, bool &outUseCustomVRAM);
	
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED> void _LineText(u16 *__restrict dstColorLine, const u16 lineIndex);
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED> void _LineRot(u16 *__restrict dstColorLine, const u16 lineIndex);
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED> u16* _LineExtRot(u16 *__restrict dstColorLine, const u16 lineIndex, bool &outUseCustomVRAM);
	
	template <GPULayerID LAYERID> void _RenderPixel_CheckWindows(const size_t srcX, bool &didPassWindowTest, bool &enableColorEffect) const;
	
	void _RenderLine_Clear(const u16 clearColor, const u16 l, u16 *renderLineTarget);
	u16* _RenderLine_Layers(const u16 l);
	
	void _HandleDisplayModeOff(const size_t l);
	void _HandleDisplayModeNormal(const size_t l);
	
	template<size_t WIN_NUM> void _UpdateWINH();
	template<size_t WIN_NUM> void _SetupWindows(const u16 lineIndex);
	
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED> u16* _RenderLine_LayerBG_Final(u16 *dstColorLine, const u16 lineIndex);
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED> u16* _RenderLine_LayerBG_ApplyColorEffectDisabledHint(u16 *dstColorLine, const u16 lineIndex);
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool ISCUSTOMRENDERINGNEEDED> u16* _RenderLine_LayerBG_ApplyNoWindowsEnabledHint(u16 *dstColorLine, const u16 lineIndex);
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool ISCUSTOMRENDERINGNEEDED> u16* _RenderLine_LayerBG_ApplyMosaic(u16 *dstColorLine, const u16 lineIndex);
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool ISCUSTOMRENDERINGNEEDED> u16* _RenderLine_LayerBG(u16 *dstColorLine, const u16 lineIndex);
			
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT> FORCEINLINE void _RenderPixel(const size_t srcX, const u16 src, const u8 srcAlpha, u16 *__restrict dstColorLine, u8 *__restrict dstLayerIDLine);
	FORCEINLINE void _RenderPixel3D(const size_t srcX, const FragmentColor src, u16 *__restrict dstColorLine, u8 *__restrict dstLayerIDLine);
	
	FORCEINLINE u16 _ColorEffectBlend(const u16 colA, const u16 colB, const u16 blendEVA, const u16 blendEVB);
	FORCEINLINE u16 _ColorEffectBlend(const u16 colA, const u16 colB, const TBlendTable *blendTable);
	
	FORCEINLINE u16 _ColorEffectBlend3D(const FragmentColor colA, const u16 colB);
	FORCEINLINE FragmentColor _ColorEffectBlend3D(const FragmentColor colA, const FragmentColor colB);
	
	FORCEINLINE u16 _ColorEffectIncreaseBrightness(const u16 col);
	FORCEINLINE u16 _ColorEffectIncreaseBrightness(const u16 col, const u16 blendEVY);
	
	FORCEINLINE u16 _ColorEffectDecreaseBrightness(const u16 col);
	FORCEINLINE u16 _ColorEffectDecreaseBrightness(const u16 col, const u16 blendEVY);
	
#ifdef ENABLE_SSE2
	FORCEINLINE __m128i _ColorEffectBlend(const __m128i &colA, const __m128i &colB, const __m128i &blendEVA, const __m128i &blendEVB);
	FORCEINLINE __m128i _ColorEffectBlend3D(const __m128i &colA_Lo, const __m128i &colA_Hi, const __m128i &colB);
	FORCEINLINE __m128i _ColorEffectIncreaseBrightness(const __m128i &col, const __m128i &blendEVY);
	FORCEINLINE __m128i _ColorEffectDecreaseBrightness(const __m128i &col, const __m128i &blendEVY);
	template<GPULayerID LAYERID, bool ISCUSTOMRENDERINGNEEDED> FORCEINLINE void _RenderPixel_CheckWindows16_SSE2(const size_t dstX, __m128i &didPassWindowTest, __m128i &enableColorEffect) const;
	template<GPULayerID LAYERID, bool ISCUSTOMRENDERINGNEEDED> FORCEINLINE void _RenderPixel_CheckWindows8_SSE2(const size_t dstX, __m128i &didPassWindowTest, __m128i &enableColorEffect) const;
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED> FORCEINLINE void _RenderPixel16_SSE2(const size_t dstX, const __m128i &srcColorHi_vec128, const __m128i &srcColorLo_vec128, const __m128i &srcOpaqueMask, const u8 *__restrict srcAlpha, u16 *__restrict dstColorLine, u8 *__restrict dstLayerIDLine);
	template <GPULayerID LAYERID, bool ISDEBUGRENDER, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED> FORCEINLINE void _RenderPixel8_SSE2(const size_t dstX, const __m128i &srcColor_vec128, const __m128i &srcOpaqueMask, const u8 *__restrict srcAlpha, u16 *__restrict dstColorLine, u8 *__restrict dstLayerIDLine);
	template<bool ISCUSTOMRENDERINGNEEDED> FORCEINLINE void _RenderPixel3D_SSE2(const size_t srcX, const FragmentColor *__restrict src, u16 *__restrict dstColorLine, u8 *__restrict dstLayerIDLine);
#endif
	
	template<bool ISDEBUGRENDER> void _RenderSpriteBMP(const u8 spriteNum, const u16 l, u16 *__restrict dst, const u32 srcadr, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha);
	template<bool ISDEBUGRENDER> void _RenderSprite256(const u8 spriteNum, const u16 l, u16 *__restrict dst, const u32 srcadr, const u16 *__restrict pal, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha);
	template<bool ISDEBUGRENDER> void _RenderSprite16(const u8 spriteNum, const u16 l, u16 *__restrict dst, const u32 srcadr, const u16 *__restrict pal, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha);
	void _RenderSpriteWin(const u8 *src, const bool col256, const size_t lg, size_t sprX, size_t x, const s32 xdir);
	bool _ComputeSpriteVars(const OAMAttributes &spriteInfo, const u16 l, SpriteSize &sprSize, s32 &sprX, s32 &sprY, s32 &x, s32 &y, s32 &lg, s32 &xdir);
	
	u32 _SpriteAddressBMP(const OAMAttributes &spriteInfo, const SpriteSize sprSize, const s32 y);
	
	template<bool ISDEBUGRENDER> void _SpriteRender(const u16 lineIndex, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab);
	template<SpriteRenderMode MODE, bool ISDEBUGRENDER> void _SpriteRenderPerform(const u16 lineIndex, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab);
	
public:
	GPUEngineBase();
	virtual ~GPUEngineBase();
	
	virtual void Reset();
	virtual void RenderLine(const u16 l);
	
	void RefreshAffineStartRegs();
	
	template<GPUEngineID ENGINEID> void ParseReg_DISPCNT();
	template<GPUEngineID ENGINEID, GPULayerID LAYERID> void ParseReg_BGnCNT();
	template<GPULayerID LAYERID> void ParseReg_BGnHOFS();
	template<GPULayerID LAYERID> void ParseReg_BGnVOFS();
	template<GPULayerID LAYERID> void ParseReg_BGnX();
	template<GPULayerID LAYERID> void ParseReg_BGnY();
	template<size_t WINNUM> void ParseReg_WINnH();
	void ParseReg_MOSAIC();
	void ParseReg_BLDCNT();
	void ParseReg_BLDALPHA();
	void ParseReg_BLDY();
	void ParseReg_MASTER_BRIGHT();
	
	template<GPUEngineID ENGINEID> void ParseAllRegisters();
	
	void UpdatePropertiesWithoutRender(const u16 l);
	void FramebufferPostprocess();
	
	bool isCustomRenderingNeeded;
	u8 vramBGLayer;
	u8 vramBlockBGIndex;
	u8 vramBlockOBJIndex;
	
	size_t nativeLineRenderCount;
	size_t nativeLineOutputCount;
	bool isLineRenderNative[GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	bool isLineOutputNative[GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	
	u16 *customBuffer;
	u16 *nativeBuffer;
	
	size_t renderedWidth;
	size_t renderedHeight;
	u16 *renderedBuffer;
	
	IOREG_BG2X savedBG2X;
	IOREG_BG2Y savedBG2Y;
	IOREG_BG3X savedBG3X;
	IOREG_BG3Y savedBG3Y;
	
	const GPU_IOREG& GetIORegisterMap() const;
	
	bool GetIsMasterBrightFullIntensity() const;
	
	bool GetEnableState();
	void SetEnableState(bool theState);
	bool GetLayerEnableState(const size_t layerIndex);
	void SetLayerEnableState(const size_t layerIndex, bool theState);
	
	template<bool ISFULLINTENSITYHINT> void ApplyMasterBrightness();
	
	const BGLayerInfo& GetBGLayerInfoByID(const GPULayerID layerID);
	
	void UpdateVRAM3DUsageProperties_BGLayer(const size_t bankIndex);
	void UpdateVRAM3DUsageProperties_OBJLayer(const size_t bankIndex);
	
	void SpriteRenderDebug(const u16 lineIndex, u16 *dst);
	template<GPULayerID LAYERID> void RenderLayerBG(u16 *dstLineColor);

	NDSDisplayID GetDisplayByID() const;
	void SetDisplayByID(const NDSDisplayID theDisplayID);
	
	GPUEngineID GetEngineID() const;
	
	virtual void SetCustomFramebufferSize(size_t w, size_t h);
	void ResolveCustomRendering();
	void ResolveToCustomFramebuffer();
	
	void REG_DISPx_pack_test();
};

class GPUEngineA : public GPUEngineBase
{
private:
	GPUEngineA();
	~GPUEngineA();
	
protected:
	CACHE_ALIGN u16 _VRAMNativeBlockCaptureCopy[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES * 4];
	u16 *_VRAMNativeBlockCaptureCopyPtr[4];
	
	FragmentColor *_3DFramebufferRGBA6665;
	u16 *_3DFramebufferRGBA5551;
	
	u16 *_VRAMNativeBlockPtr[4];
	u16 *_VRAMCustomBlockPtr[4];
	
	DISPCAPCNT_parsed _dispCapCnt;
	
	template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED> void _LineLarge8bpp(u16 *__restrict dstColorLine, const u16 lineIndex);
	
	u16* _RenderLine_Layers(const u16 l);
	template<size_t CAPTURELENGTH> void _RenderLine_DisplayCapture(const u16 *renderedLineSrcA, const u16 l);
	void _RenderLine_DispCapture_FIFOToBuffer(u16 *fifoLineBuffer);
	
	template<int SOURCESWITCH, size_t CAPTURELENGTH, bool CAPTUREFROMNATIVESRC, bool CAPTURETONATIVEDST>
	void _RenderLine_DispCapture_Copy(const u16 *src, u16 *dst, const size_t captureLengthExt, const size_t captureLineCount); // Do not use restrict pointers, since src and dst can be the same
	
	u16 _RenderLine_DispCapture_BlendFunc(const u16 srcA, const u16 srcB, const u8 blendEVA, const u8 blendEVB);
	
#ifdef ENABLE_SSE2
	__m128i _RenderLine_DispCapture_BlendFunc_SSE2(__m128i &srcA, __m128i &srcB, const __m128i &blendEVA, const __m128i &blendEVB);
#endif
	
	template<bool CAPTUREFROMNATIVESRCA, bool CAPTUREFROMNATIVESRCB>
	void _RenderLine_DispCapture_BlendToCustomDstBuffer(const u16 *srcA, const u16 *srcB, u16 *dst, const u8 blendEVA, const u8 blendEVB, const size_t length, size_t l); // Do not use restrict pointers, since srcB and dst can be the same
	
	template<size_t CAPTURELENGTH, bool CAPTUREFROMNATIVESRCA, bool CAPTUREFROMNATIVESRCB, bool CAPTURETONATIVEDST>
	void _RenderLine_DispCapture_Blend(const u16 *srcA, const u16 *srcB, u16 *dst, const size_t captureLengthExt, const size_t l); // Do not use restrict pointers, since srcB and dst can be the same
	
	void _HandleDisplayModeVRAM(const size_t l);
	void _HandleDisplayModeMainMemory(const size_t l);
	
public:
	static GPUEngineA* Allocate();
	void FinalizeAndDeallocate();
	
	size_t nativeLineCaptureCount[4];
	bool isLineCaptureNative[4][GPU_VRAM_BLOCK_LINES];
	
	void ParseReg_DISPCAPCNT();
	u16* GetCustomVRAMBlockPtr(const size_t blockID);
	FragmentColor* Get3DFramebufferRGBA6665() const;
	u16* Get3DFramebufferRGBA5551() const;
	virtual void SetCustomFramebufferSize(size_t w, size_t h);
	
	bool WillRender3DLayer();
	bool WillCapture3DLayerDirect();
	bool VerifyVRAMLineDidChange(const size_t blockID, const size_t l);
	
	void FramebufferPostprocess();
	
	virtual void Reset();
	virtual void RenderLine(const u16 l);
};

class GPUEngineB : public GPUEngineBase
{
private:
	GPUEngineB();
	~GPUEngineB();
	
protected:
	u16* _RenderLine_Layers(const u16 l);
	
public:
	static GPUEngineB* Allocate();
	void FinalizeAndDeallocate();
	
	virtual void Reset();
	virtual void RenderLine(const u16 l);
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

class GPUEventHandler
{
public:
	virtual void DidFrameBegin(bool isFrameSkipRequested) = 0;
	virtual void DidFrameEnd(bool isFrameSkipped) = 0;
	virtual void DidRender3DBegin() = 0;
	virtual void DidRender3DEnd() = 0;
};

// All of the default event handler methods should do nothing.
// If a subclass doesn't need to override every method, then it might be easier
// if you subclass GPUEventHandlerDefault instead of GPUEventHandler.
class GPUEventHandlerDefault : public GPUEventHandler
{
public:
	virtual void DidFrameBegin(bool isFrameSkipRequested) {};
	virtual void DidFrameEnd(bool isFrameSkipped) {};
	virtual void DidRender3DBegin() {};
	virtual void DidRender3DEnd() {};
};

class GPUSubsystem
{
private:
	GPUSubsystem();
	~GPUSubsystem();
	
	GPUEventHandlerDefault *_defaultEventHandler;
	GPUEventHandler *_event;
	
	GPUEngineA *_engineMain;
	GPUEngineB *_engineSub;
	NDSDisplay *_displayMain;
	NDSDisplay *_displayTouch;
	
	bool _willAutoResolveToCustomBuffer;
	u16 *_customVRAM;
	u16 *_customVRAMBlank;
	
	CACHE_ALIGN u16 _nativeFramebuffer[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2];
	u16 *_customFramebuffer;
	
	NDSDisplayInfo _displayInfo;
	
public:
	static GPUSubsystem* Allocate();
	void FinalizeAndDeallocate();
	
	void SetEventHandler(GPUEventHandler *eventHandler);
	GPUEventHandler* GetEventHandler();
	
	void Reset();
	const NDSDisplayInfo& GetDisplayInfo(); // Frontends need to call this whenever they need to read the video buffers from the emulator core
	void SetDisplayDidCustomRender(NDSDisplayID displayID, bool theState);
	
	GPUEngineA* GetEngineMain();
	GPUEngineB* GetEngineSub();
	NDSDisplay* GetDisplayMain();
	NDSDisplay* GetDisplayTouch();
	
	u16* GetCustomVRAMBuffer();
	u16* GetCustomVRAMBlankBuffer();
	u16* GetCustomVRAMAddressUsingMappedAddress(const u32 addr);
	
	size_t GetCustomFramebufferWidth() const;
	size_t GetCustomFramebufferHeight() const;
	void SetCustomFramebufferSize(size_t w, size_t h, u16 *clientNativeBuffer, u16 *clientCustomBuffer);
	void SetCustomFramebufferSize(size_t w, size_t h);
	
	void UpdateRenderProperties();
	
	// Normally, the GPUs will automatically resolve their native buffers to the master
	// custom framebuffer at the end of V-blank so that all rendered graphics are contained
	// within a single common buffer. This is necessary for when someone wants to read
	// the NDS framebuffers, but the reader can only read a single buffer at a time.
	// Certain functions, such as taking screenshots, as well as many frontends running
	// the NDS video displays, require that they read from only a single buffer.
	//
	// However, if SetWillAutoResolveToCustomBuffer() is passed "false", then the
	// frontend becomes responsible for calling GetDisplayInfo() and reading the native
	// and custom buffers properly for each display. If a single buffer is still needed
	// for certain cases, then the frontend must manually call
	// GPUEngineBase::ResolveToCustomFramebuffer() for each engine before reading the
	// master custom framebuffer.
	bool GetWillAutoResolveToCustomBuffer() const;
	void SetWillAutoResolveToCustomBuffer(const bool willAutoResolve);
	
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
