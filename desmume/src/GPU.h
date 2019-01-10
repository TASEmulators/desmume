/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 Theo Berkau
	Copyright (C) 2007 shash
	Copyright (C) 2009-2019 DeSmuME team

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
#include "./utils/colorspacehandler/colorspacehandler.h"

#ifdef ENABLE_SSE2
#include <emmintrin.h>
#include "./utils/colorspacehandler/colorspacehandler_SSE2.h"
#endif

#ifdef ENABLE_SSSE3
#include <tmmintrin.h>
#endif

#ifdef ENABLE_SSE4_1
#include <smmintrin.h>
#endif

// Note: Technically, the shift count of palignr can be any value of [0-255]. But practically speaking, the
// shift count should be a value of [0-15]. If we assume that the value range will always be [0-15], we can
// then substitute the palignr instruction with an SSE2 equivalent.
#if defined(ENABLE_SSE2) && !defined(ENABLE_SSSE3)
	#define _mm_alignr_epi8(a, b, immShiftCount) _mm_or_si128(_mm_slli_si128(a, 16-(immShiftCount)), _mm_srli_si128(b, (immShiftCount)))
#endif

// Note: The SSE4.1 version of pblendvb only requires that the MSBs of the 8-bit mask vector are set in order to
// pass the b byte through. However, our SSE2 substitute of pblendvb requires that all of the bits of the 8-bit
// mask vector are set. So when using this intrinsic in practice, just set/clear all mask bits together, and it
// should work fine for both SSE4.1 and SSE2.
#if defined(ENABLE_SSE2) && !defined(ENABLE_SSE4_1)
	#define _mm_blendv_epi8(a, b, fullmask) _mm_or_si128(_mm_and_si128((fullmask), (b)), _mm_andnot_si128((fullmask), (a)))
#endif

class GPUEngineBase;
class EMUFILE;
class Task;
struct MMU_struct;
struct Render3DInterface;

//#undef FORCEINLINE
//#define FORCEINLINE

#define GPU_FRAMEBUFFER_NATIVE_WIDTH	256
#define GPU_FRAMEBUFFER_NATIVE_HEIGHT	192

#define GPU_VRAM_BLOCK_LINES			256
#define GPU_VRAM_BLANK_REGION_LINES		544

#define MAX_FRAMEBUFFER_PAGES			8

void gpu_savestate(EMUFILE &os);
bool gpu_loadstate(EMUFILE &is, int size);

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

typedef union
{
    u32 value;
	
	struct
	{
#ifndef MSB_FIRST
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
#ifndef MSB_FIRST
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
	s32 value;
	
	struct
	{
#ifndef MSB_FIRST
		u32 Fraction:8;
		s32 Integer:20;
		s32 :4;
#else
		s32 :4;
		s32 Integer:20;
		u32 Fraction:8;
#endif
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
#ifndef MSB_FIRST
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
#ifndef MSB_FIRST
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
#ifndef MSB_FIRST
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
#ifndef MSB_FIRST
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
#ifndef MSB_FIRST
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
#ifndef MSB_FIRST
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
#ifndef MSB_FIRST
		u32 EVA:5;							//  0- 4: Blending coefficient for SrcA; 0...31 (clamped to 16)
		u32 :3;								//  5- 7: Unused bits
		
		u32 EVB:5;							//  8-12: Blending coefficient for SrcB; 0...31 (clamped to 16)
		u32 :3;								// 13-15: Unused bits
		
		u32 VRAMWriteBlock:2;				// 16-17: VRAM write target block; 0=Block A, 1=Block B, 2=Block C, 3=Block D
		u32 VRAMWriteOffset:2;				// 18-19: VRAM write target offset; 0=0KB, 1=32KB, 2=64KB, 3=96KB
		u32 CaptureSize:2;					// 20-21: Display capture dimensions; 0=128x128, 1=256x64, 2=256x128, 3=256x192
		u32 :2;								// 22-23: Unused bits
		
		u32 SrcA:1;							//    24: SrcA target; 0=Current framebuffer, 1=3D render buffer
		u32 SrcB:1;							//    25: SrcB target;
											//        0=VRAM block, interacts with DISPCNT (0x4000000)
											//        1=Main memory FIFO, interacts with DISP_MMEM_FIFO (0x4000068)
		u32 VRAMReadOffset:2;				// 26-27: VRAM read target offset; 0=0KB, 1=32KB, 2=64KB, 3=96KB
		u32 :1;								//    28: Unused bit
		u32 CaptureSrc:2;					// 29-30: Select capture target; 0=SrcA, 1=SrcB, 2=SrcA+SrcB blend, 3=SrcA+SrcB blend
		u32 CaptureEnable:1;				//    31: Display capture status; 0=Disable/Ready 1=Enable/Busy
#else
		u32 :3;								//  5- 7: Unused bits
		u32 EVA:5;							//  0- 4: Blending coefficient for SrcA; 0...31 (clamped to 16)
		
		u32 :3;								// 13-15: Unused bits
		u32 EVB:5;							//  8-12: Blending coefficient for SrcB; 0...31 (clamped to 16)
		
		u32 :2;								// 22-23: Unused bits
		u32 CaptureSize:2;					// 20-21: Display capture dimensions; 0=128x128, 1=256x64, 2=256x128, 3=256x192
		u32 VRAMWriteOffset:2;				// 18-19: VRAM write target offset; 0=0KB, 1=32KB, 2=64KB, 3=96KB
		u32 VRAMWriteBlock:2;				// 16-17: VRAM write target block; 0=Block A, 1=Block B, 2=Block C, 3=Block D
		
		u32 CaptureEnable:1;				//    31: Display capture status; 0=Disable/Ready 1=Enable/Busy
		u32 CaptureSrc:2;					// 29-30: Select capture target; 0=SrcA, 1=SrcB, 2=SrcA+SrcB blend, 3=SrcA+SrcB blend
		u32 :1;								//    28: Unused bit
		u32 VRAMReadOffset:2;				// 26-27: VRAM read target offset; 0=0KB, 1=32KB, 2=64KB, 3=96KB
		u32 SrcB:1;							//    25: SrcB target;
											//        0=VRAM block, interacts with DISPCNT (0x4000000)
											//        1=Main memory FIFO, interacts with DISP_MMEM_FIFO (0x4000068)
		u32 SrcA:1;							//    24: SrcA target; 0=Current framebuffer, 1=3D render buffer
#endif
	};
	
} IOREG_DISPCAPCNT;							// 0x4000064: Display capture control (Engine A only)

typedef u32 IOREG_DISP_MMEM_FIFO;			// 0x4000068: Main memory FIFO data (Engine A only)

typedef union
{
	u32 value;
	
	struct
	{
#ifndef MSB_FIRST
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

typedef union
{
	u8 value;
	
	struct
	{
#ifndef MSB_FIRST
		u8 SoundAmp_Enable:1;						//  0: Sound amplifier; 0=Disable, 1=Enable
		u8 SoundAmp_Mute:1;							//  1: Sound amplifier mute control; 0=Normal, 1=Mute
		u8 TouchBacklight_Enable:1;					//  2: Touch display backlight; 0=Disable, 1=Enable
		u8 MainBacklight_Enable:1;					//  3: Main display backlight; 0=Disable, 1=Enable
		u8 PowerLEDBlink_Enable:1;					//  4: Power LED blink control; 0=Disable (Power LED remains steadily ON), 1=Enable
		u8 PowerLEDBlinkSpeed:1;					//  5: Power LED blink speed when enabled; 0=Slow, 1=Fast
		u8 SystemPowerState:1;						//  6: NDS system power state; 0=System is powered ON, 1=System is powered OFF
		u8 :1;										//  7: Unused bit (should always be read as 0)
#else
		u8 :1;										//  7: Unused bit (should always be read as 0)
		u8 SystemPowerState:1;						//  6: NDS system power state; 0=System is powered ON, 1=System is powered OFF
		u8 PowerLEDBlinkSpeed:1;					//  5: Power LED blink speed when enabled; 0=Slow, 1=Fast
		u8 PowerLEDBlink_Enable:1;					//  4: Power LED blink control; 0=Disable (LED is steady ON), 1=Enable
		u8 MainBacklight_Enable:1;					//  3: Main display backlight; 0=Disable, 1=Enable
		u8 TouchBacklight_Enable:1;					//  2: Touch display backlight; 0=Disable, 1=Enable
		u8 SoundAmp_Mute:1;							//  1: Sound amplifier mute control; 0=Normal, 1=Mute
		u8 SoundAmp_Enable:1;						//  0: Sound amplifier; 0=Disable, 1=Enable
#endif
	};
} IOREG_POWERMANCTL;

typedef union
{
	u8 value;
	
	struct
	{
#ifndef MSB_FIRST
		u8 Level:2;									// 0-1: Backlight brightness level;
													//      0=Low
													//      1=Medium
													//      2=High
													//      3=Maximum
		u8 ForceMaxBrightnessWithExtPower_Enable:1;	//   2: Forces maximum brightness if external power is present, interacts with Bit 3; 0=Disable, 1=Enable
		u8 ExternalPowerState:1;					//   3: External power state; 0=External power is not present, 1=External power is present
		u8 :4;										// 4-7: Unknown bits (should always be read as 4)
#else
		u8 :4;										// 4-7: Unknown bits (should always be read as 4)
		u8 ExternalPowerState:1;					//   3: External power state; 0=External power is not present, 1=External power is present
		u8 ForceMaxBrightnessWithExtPower_Enable:1;	//   2: Forces maximum brightness if external power is present, interacts with Bit 3; 0=Disable, 1=Enable
		u8 Level:2;									// 0-1: Backlight brightness level;
													//      0=Low
													//      1=Medium
													//      2=High
													//      3=Maximum
#endif
	};
} IOREG_BACKLIGHTCTL;

enum BacklightLevel
{
	BacklightLevel_Low					= 0,
	BacklightLevel_Medium				= 1,
	BacklightLevel_High					= 2,
	BacklightLevel_Maximum				= 3
};

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

#ifdef MSB_FIRST
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
#ifdef MSB_FIRST
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
#ifndef MSB_FIRST
		union
		{
			u16 attr0;
			
			struct
			{
				u16 Y:8;					//  0- 7: Sprite Y-coordinate location within the framebuffer; 0...255
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
		
		s16 X:9;							// 16-24: Sprite X-coordinate location within the framebuffer; 0...511
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
				u16 Y:8;					//  0- 7: Sprite Y-coordinate location within the framebuffer; 0...255
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
		s16 X:9;							// 16-24: Sprite X-coordinate location within the framebuffer; 0...511
		
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

#define WINDOWCONTROL_EFFECTFLAG	5

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

enum GPUCompositorMode
{
	GPUCompositorMode_Debug			= 0,
	GPUCompositorMode_Copy			= 1,
	GPUCompositorMode_BrightUp		= 2,
	GPUCompositorMode_BrightDown	= 3,
	GPUCompositorMode_Unknown		= 100
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
	// User-requested settings. These fields will always remain constant until changed.
	
	// Changed by calling GPUSubsystem::SetColorFormat().
	NDSColorFormat colorFormat;					// The output color format.
	size_t pixelBytes;							// The number of bytes per pixel.
	
	// Changed by calling GPUSubsystem::SetFramebufferSize().
	bool isCustomSizeRequested;					// Reports that the call to GPUSubsystem::SetFramebufferSize() resulted in a custom rendering size.
												//    true  - The user requested a custom size.
												//    false - The user requested the native size.
	size_t customWidth;							// The requested custom width, measured in pixels.
	size_t customHeight;						// The requested custom height, measured in pixels.
	size_t framebufferPageSize;					// The size of a single framebuffer page, which includes the native and custom buffers of both
												// displays, measured in bytes.
	
	// Changed by calling GPUSubsystem::SetColorFormat() or GPUSubsystem::SetFramebufferSize().
	size_t framebufferPageCount;				// The number of framebuffer pages that were requested by the client.
	void *masterFramebufferHead;				// Pointer to the head of the master framebuffer memory block that encompasses all buffers.
	
	// Changed by calling GPUEngineBase::SetEnableState().
	bool isDisplayEnabled[2];					// Reports that a particular display has been enabled or disabled by the user.
	
	
	
	// Frame render state information. These fields will change per frame, depending on how each display was rendered.
	u8 bufferIndex;								// Index of a specific framebuffer page for the GPU emulation to write data into.
												// Indexing starts at 0, and must be less than framebufferPageCount.
												// A specific index can be chosen at the DidFrameBegin event.
	size_t sequenceNumber;						// A unique number assigned to each frame that increments for each DidFrameEnd event. Never resets.
	
	void *masterNativeBuffer;					// Pointer to the head of the master native buffer.
	void *masterCustomBuffer;					// Pointer to the head of the master custom buffer.
												// If GPUSubsystem::GetWillAutoResolveToCustomBuffer() would return true, or if
												// GPUSubsystem::ResolveDisplayToCustomFramebuffer() is called, then this buffer is used as the
												// target buffer for resolving any native-sized renders.
	
	void *nativeBuffer[2];						// Pointer to the display's native size framebuffer.
	void *customBuffer[2];						// Pointer to the display's custom size framebuffer.
	
	size_t renderedWidth[2];					// The display rendered at this width, measured in pixels.
	size_t renderedHeight[2];					// The display rendered at this height, measured in pixels.
	void *renderedBuffer[2];					// The display rendered to this buffer.
	
	GPUEngineID engineID[2];					// ID of the engine sourcing the display.
	
	bool didPerformCustomRender[2];				// Reports that the display actually rendered at a custom size for this frame.
												//    true  - The display performed a custom-sized render.
												//    false - The display performed a native-sized render.
	
	bool masterBrightnessDiffersPerLine[2];		// Reports if the master brightness values may differ per scanline. If true, then it will
												// need to be applied on a per-scanline basis. Otherwise, it can be applied to the entire
												// framebuffer. For most NDS games, this field will be false.
	u8 masterBrightnessMode[2][GPU_FRAMEBUFFER_NATIVE_HEIGHT]; // The master brightness mode of each display line.
	u8 masterBrightnessIntensity[2][GPU_FRAMEBUFFER_NATIVE_HEIGHT]; // The master brightness intensity of each display line.
	
	float backlightIntensity[2];				// Reports the intensity of the backlight.
												//    0.000 - The backlight is completely off.
												//    1.000 - The backlight is at its maximum brightness.
	
	
	
	// Postprocessing information. These fields report the status of each postprocessing step.
	// Typically, these fields should be modified whenever GPUSubsystem::PostprocessDisplay() is called.
	bool needConvertColorFormat[2];				// Reports if the display still needs to convert its color format from RGB666 to RGB888.
	bool needApplyMasterBrightness[2];			// Reports if the display still needs to apply the master brightness.
} NDSDisplayInfo;

typedef struct
{
	u8 begin;
	u8 trunc;
} MosaicTableEntry;

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

typedef struct
{
	size_t indexNative;
	size_t indexCustom;
	size_t widthCustom;
	size_t renderCount;
	size_t pixelCount;
	size_t blockOffsetNative;
	size_t blockOffsetCustom;
} GPUEngineLineInfo;

typedef struct
{
	GPULayerID previouslyRenderedLayerID;
	GPULayerID selectedLayerID;
	BGLayerInfo *selectedBGLayer;
	
	GPUDisplayMode displayOutputMode;
	u16 backdropColor16;
	u16 workingBackdropColor16;
	FragmentColor workingBackdropColor32;
	ColorEffect colorEffect;
	u8 blendEVA;
	u8 blendEVB;
	u8 blendEVY;
	
	GPUMasterBrightMode masterBrightnessMode;
	u8 masterBrightnessIntensity;
	bool masterBrightnessIsFullIntensity;
	bool masterBrightnessIsMaxOrMin;
	
	TBlendTable *blendTable555;
	u16 *brightnessUpTable555;
	FragmentColor *brightnessUpTable666;
	FragmentColor *brightnessUpTable888;
	u16 *brightnessDownTable555;
	FragmentColor *brightnessDownTable666;
	FragmentColor *brightnessDownTable888;
	
	bool srcEffectEnable[6];
	bool dstBlendEnable[6];
#ifdef ENABLE_SSE2
	__m128i srcEffectEnable_SSE2[6];
#ifdef ENABLE_SSSE3
	__m128i dstBlendEnable_SSSE3;
#else
	__m128i dstBlendEnable_SSE2[6];
#endif
#endif // ENABLE_SSE2
	bool dstAnyBlendEnable;
	
	u8 WIN0_enable[6];
	u8 WIN1_enable[6];
	u8 WINOUT_enable[6];
	u8 WINOBJ_enable[6];
#if defined(ENABLE_SSE2)
	__m128i WIN0_enable_SSE2[6];
	__m128i WIN1_enable_SSE2[6];
	__m128i WINOUT_enable_SSE2[6];
	__m128i WINOBJ_enable_SSE2[6];
#endif
	
	bool WIN0_ENABLED;
	bool WIN1_ENABLED;
	bool WINOBJ_ENABLED;
	bool isAnyWindowEnabled;
	
	MosaicTableEntry *mosaicWidthBG;
	MosaicTableEntry *mosaicHeightBG;
	MosaicTableEntry *mosaicWidthOBJ;
	MosaicTableEntry *mosaicHeightOBJ;
	bool isBGMosaicSet;
	bool isOBJMosaicSet;
	
	SpriteRenderMode spriteRenderMode;
	u8 spriteBoundary;
	u8 spriteBMPBoundary;
} GPUEngineRenderState;

typedef struct
{
	void *lineColorHead;
	void *lineColorHeadNative;
	void *lineColorHeadCustom;
	
	u8 *lineLayerIDHead;
	u8 *lineLayerIDHeadNative;
	u8 *lineLayerIDHeadCustom;
	
	size_t xNative;
	size_t xCustom;
	void **lineColor;
	u16 *lineColor16;
	FragmentColor *lineColor32;
	u8 *lineLayerID;
} GPUEngineTargetState;

typedef struct
{
	GPUEngineLineInfo line;
	GPUEngineRenderState renderState;
	GPUEngineTargetState target;
} GPUEngineCompositorInfo;

class GPUEngineBase
{
protected:
	static CACHE_ALIGN u16 _brightnessUpTable555[17][0x8000];
	static CACHE_ALIGN FragmentColor _brightnessUpTable666[17][0x8000];
	static CACHE_ALIGN FragmentColor _brightnessUpTable888[17][0x8000];
	static CACHE_ALIGN u16 _brightnessDownTable555[17][0x8000];
	static CACHE_ALIGN FragmentColor _brightnessDownTable666[17][0x8000];
	static CACHE_ALIGN FragmentColor _brightnessDownTable888[17][0x8000];
	static CACHE_ALIGN u8 _blendTable555[17][17][32][32];
	
	static const CACHE_ALIGN SpriteSize _sprSizeTab[4][4];
	static const CACHE_ALIGN BGLayerSize _BGLayerSizeLUT[8][4];
	static const CACHE_ALIGN BGType _mode2type[8][4];
	
	static struct MosaicLookup
	{
		CACHE_ALIGN MosaicTableEntry table[16][256];
		
		MosaicLookup()
		{
			for (size_t m = 0; m < 16; m++)
			{
				for (size_t i = 0; i < 256; i++)
				{
					size_t mosaic = m+1;
					MosaicTableEntry &te = table[m][i];
					te.begin = ((i % mosaic) == 0);
					te.trunc = (i / mosaic) * mosaic;
				}
			}
		}
	} _mosaicLookup;
	
	CACHE_ALIGN u16 _sprColor[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _sprAlpha[GPU_FRAMEBUFFER_NATIVE_HEIGHT][GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _sprType[GPU_FRAMEBUFFER_NATIVE_HEIGHT][GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _sprPrio[GPU_FRAMEBUFFER_NATIVE_HEIGHT][GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _sprWin[GPU_FRAMEBUFFER_NATIVE_HEIGHT][GPU_FRAMEBUFFER_NATIVE_WIDTH];
	
	CACHE_ALIGN u8 _didPassWindowTestNative[5][GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _enableColorEffectNative[5][GPU_FRAMEBUFFER_NATIVE_WIDTH];
	
	CACHE_ALIGN u8 _deferredIndexNative[GPU_FRAMEBUFFER_NATIVE_WIDTH * 4];
	CACHE_ALIGN u16 _deferredColorNative[GPU_FRAMEBUFFER_NATIVE_WIDTH * 4];
	
	bool _needExpandSprColorCustom;
	u16 *_sprColorCustom;
	u8 *_sprAlphaCustom;
	u8 *_sprTypeCustom;
	
	u8 *_didPassWindowTestCustomMasterPtr;
	u8 *_enableColorEffectCustomMasterPtr;
	u8 *_didPassWindowTestCustom[5];
	u8 *_enableColorEffectCustom[5];
	
	GPUEngineCompositorInfo _currentCompositorInfo[GPU_VRAM_BLOCK_LINES + 1];
	GPUEngineRenderState _currentRenderState;
	
	u8 *_deferredIndexCustom;
	u16 *_deferredColorCustom;
	
	void *_customBuffer;
	void *_nativeBuffer;
	size_t _renderedWidth;
	size_t _renderedHeight;
	void *_renderedBuffer;
	
	bool _enableEngine;
	bool _enableBGLayer[5];
	
	bool _isBGLayerShown[5];
	bool _isAnyBGLayerShown;
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
	GPU_IOREG *_IORegisterMap;
	u16 *_paletteBG;
	u16 *_paletteOBJ;
	OAMAttributes *_oamList;
	u32 _sprMem;
	
	BGLayerInfo _BGLayer[4];
	
	CACHE_ALIGN u8 _sprNum[256];
	CACHE_ALIGN u8 _h_win[2][GPU_FRAMEBUFFER_NATIVE_WIDTH];
	
	NDSDisplayID _targetDisplayID;
	
	CACHE_ALIGN FragmentColor _internalRenderLineTargetNative[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN u8 _renderLineLayerIDNative[GPU_FRAMEBUFFER_NATIVE_HEIGHT][GPU_FRAMEBUFFER_NATIVE_WIDTH];
	
	void *_internalRenderLineTargetCustom;
	u8 *_renderLineLayerIDCustom;
	bool _needUpdateWINH[2];
	
	Task *_asyncClearTask;
	bool _asyncClearIsRunning;
	u8 _asyncClearTransitionedLineFromBackdropCount;
	volatile s32 _asyncClearLineCustom;
	volatile s32 _asyncClearInterrupt;
	u16 _asyncClearBackdropColor16; // Do not modify this variable directly.
	FragmentColor _asyncClearBackdropColor32; // Do not modify this variable directly.
	bool _asyncClearUseInternalCustomBuffer; // Do not modify this variable directly.
	
	void _InitLUTs();
	void _Reset_Base();
	void _ResortBGLayers();
	
	template<NDSColorFormat OUTPUTFORMAT> void _TransitionLineNativeToCustom(GPUEngineCompositorInfo &compInfo);
	
	void _MosaicSpriteLinePixel(GPUEngineCompositorInfo &compInfo, const size_t x, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab);
	void _MosaicSpriteLine(GPUEngineCompositorInfo &compInfo, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab);
	
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING, PixelLookupFunc GetPixelFunc, bool WRAP> void _RenderPixelIterate_Final(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal);
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING, PixelLookupFunc GetPixelFunc, bool WRAP> void _RenderPixelIterate_ApplyWrap(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal);
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING, PixelLookupFunc GetPixelFunc> void _RenderPixelIterate(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal);
	
	TILEENTRY _GetTileEntry(const u32 tileMapAddress, const u16 xOffset, const u16 layerWidthMask);
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST> FORCEINLINE void _CompositePixelImmediate(GPUEngineCompositorInfo &compInfo, const size_t srcX, u16 srcColor16, bool opaque);
	template<bool MOSAIC> void _PrecompositeNativeToCustomLineBG(GPUEngineCompositorInfo &compInfo);
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST> void _CompositeNativeLineOBJ(GPUEngineCompositorInfo &compInfo, const u16 *__restrict srcColorNative16, const FragmentColor *__restrict srcColorNative32);
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST> void _CompositeLineDeferred(GPUEngineCompositorInfo &compInfo, const u16 *__restrict srcColorCustom16, const u8 *__restrict srcIndexCustom);
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST> void _CompositeVRAMLineDeferred(GPUEngineCompositorInfo &compInfo, const void *__restrict vramColorPtr);
	
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING> void _RenderLine_BGText(GPUEngineCompositorInfo &compInfo, const u16 XBG, const u16 YBG);
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING> void _RenderLine_BGAffine(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param);
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING> void _RenderLine_BGExtended(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param, bool &outUseCustomVRAM);
	
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING> void _LineText(GPUEngineCompositorInfo &compInfo);
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING> void _LineRot(GPUEngineCompositorInfo &compInfo);
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING> void _LineExtRot(GPUEngineCompositorInfo &compInfo, bool &outUseCustomVRAM);
	
	template<NDSColorFormat OUTPUTFORMAT> void _RenderLine_Clear(GPUEngineCompositorInfo &compInfo);
	void _RenderLine_SetupSprites(GPUEngineCompositorInfo &compInfo);
	template<NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST> void _RenderLine_Layers(GPUEngineCompositorInfo &compInfo);
	
	template<NDSColorFormat OUTPUTFORMAT> void _HandleDisplayModeOff(const size_t l);
	template<NDSColorFormat OUTPUTFORMAT> void _HandleDisplayModeNormal(const size_t l);
	
	template<size_t WIN_NUM> void _UpdateWINH(GPUEngineCompositorInfo &compInfo);
	template<size_t WIN_NUM> bool _IsWindowInsideVerticalRange(GPUEngineCompositorInfo &compInfo);
	void _PerformWindowTesting(GPUEngineCompositorInfo &compInfo);
	
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING> FORCEINLINE void _RenderLine_LayerBG_Final(GPUEngineCompositorInfo &compInfo);
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST> FORCEINLINE void _RenderLine_LayerBG_ApplyMosaic(GPUEngineCompositorInfo &compInfo);
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST> void _RenderLine_LayerBG(GPUEngineCompositorInfo &compInfo);
	
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST> void _RenderLine_LayerOBJ(GPUEngineCompositorInfo &compInfo, itemsForPriority_t *__restrict item);
	
	template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER> FORCEINLINE void _PixelCopy(GPUEngineCompositorInfo &compInfo, const u16 srcColor16);
	template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER> FORCEINLINE void _PixelCopy(GPUEngineCompositorInfo &compInfo, const FragmentColor srcColor32);
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _PixelBrightnessUp(GPUEngineCompositorInfo &compInfo, const u16 srcColor16);
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _PixelBrightnessUp(GPUEngineCompositorInfo &compInfo, const FragmentColor srcColor32);
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _PixelBrightnessDown(GPUEngineCompositorInfo &compInfo, const u16 srcColor16);
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _PixelBrightnessDown(GPUEngineCompositorInfo &compInfo, const FragmentColor srcColor32);
	template<NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE> FORCEINLINE void _PixelUnknownEffect(GPUEngineCompositorInfo &compInfo, const u16 srcColor16, const bool enableColorEffect, const u8 spriteAlpha, const OBJMode spriteMode);
	template<NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE> FORCEINLINE void _PixelUnknownEffect(GPUEngineCompositorInfo &compInfo, const FragmentColor srcColor32, const bool enableColorEffect, const u8 spriteAlpha, const OBJMode spriteMode);
	
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE> FORCEINLINE void _PixelComposite(GPUEngineCompositorInfo &compInfo, const u16 srcColor16, const bool enableColorEffect, const u8 spriteAlpha, const u8 spriteMode);
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE> FORCEINLINE void _PixelComposite(GPUEngineCompositorInfo &compInfo, FragmentColor srcColor32, const bool enableColorEffect, const u8 spriteAlpha, const u8 spriteMode);
	
	FORCEINLINE u16 _ColorEffectBlend(const u16 colA, const u16 colB, const u16 blendEVA, const u16 blendEVB);
	FORCEINLINE u16 _ColorEffectBlend(const u16 colA, const u16 colB, const TBlendTable *blendTable);
	template<NDSColorFormat COLORFORMAT> FORCEINLINE FragmentColor _ColorEffectBlend(const FragmentColor colA, const FragmentColor colB, const u16 blendEVA, const u16 blendEVB);
	
	FORCEINLINE u16 _ColorEffectBlend3D(const FragmentColor colA, const u16 colB);
	template<NDSColorFormat COLORFORMATB> FORCEINLINE FragmentColor _ColorEffectBlend3D(const FragmentColor colA, const FragmentColor colB);
	
	FORCEINLINE u16 _ColorEffectIncreaseBrightness(const u16 col, const u16 blendEVY);
	template<NDSColorFormat COLORFORMAT> FORCEINLINE FragmentColor _ColorEffectIncreaseBrightness(const FragmentColor col, const u16 blendEVY);
	
	FORCEINLINE u16 _ColorEffectDecreaseBrightness(const u16 col, const u16 blendEVY);
	FORCEINLINE FragmentColor _ColorEffectDecreaseBrightness(const FragmentColor col, const u16 blendEVY);
	
#ifdef ENABLE_SSE2
	template<NDSColorFormat COLORFORMAT, bool USECONSTANTBLENDVALUESHINT> FORCEINLINE __m128i _ColorEffectBlend(const __m128i &colA, const __m128i &colB, const __m128i &blendEVA, const __m128i &blendEVB);
	template<NDSColorFormat COLORFORMATB> FORCEINLINE __m128i _ColorEffectBlend3D(const __m128i &colA_Lo, const __m128i &colA_Hi, const __m128i &colB);
	template<NDSColorFormat COLORFORMAT> FORCEINLINE __m128i _ColorEffectIncreaseBrightness(const __m128i &col, const __m128i &blendEVY);
	template<NDSColorFormat COLORFORMAT> FORCEINLINE __m128i _ColorEffectDecreaseBrightness(const __m128i &col, const __m128i &blendEVY);
	template<bool WILLDEFERCOMPOSITING> FORCEINLINE void _RenderPixel_CheckWindows16_SSE2(GPUEngineCompositorInfo &compInfo, const size_t dstX, __m128i &didPassWindowTest, __m128i &enableColorEffect) const;
	
	template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER> FORCEINLINE void _PixelCopy16_SSE2(GPUEngineCompositorInfo &compInfo, const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0, __m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0, __m128i &dstLayerID);
	template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER> FORCEINLINE void _PixelCopyWithMask16_SSE2(GPUEngineCompositorInfo &compInfo, const __m128i &passMask8, const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0, __m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0, __m128i &dstLayerID);
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _PixelBrightnessUp16_SSE2(GPUEngineCompositorInfo &compInfo, const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0, __m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0, __m128i &dstLayerID);
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _PixelBrightnessUpWithMask16_SSE2(GPUEngineCompositorInfo &compInfo, const __m128i &passMask8, const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0, __m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0, __m128i &dstLayerID);
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _PixelBrightnessDown16_SSE2(GPUEngineCompositorInfo &compInfo, const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0, __m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0, __m128i &dstLayerID);
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _PixelBrightnessDownWithMask16_SSE2(GPUEngineCompositorInfo &compInfo, const __m128i &passMask8, const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0, __m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0, __m128i &dstLayerID);
	
	template<NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
	FORCEINLINE void _PixelUnknownEffectWithMask16_SSE2(GPUEngineCompositorInfo &compInfo,
														const __m128i &passMask8,
														const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0,
														const __m128i &srcEffectEnableMask,
														const __m128i &enableColorEffectMask,
														const __m128i &spriteAlpha,
														const __m128i &spriteMode,
														__m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0,
														__m128i &dstLayerID);
	
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
	FORCEINLINE void _PixelComposite16_SSE2(GPUEngineCompositorInfo &compInfo,
											const bool didAllPixelsPass,
											const __m128i &passMask8,
											const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0,
											const __m128i &srcEffectEnableMask,
											const u8 *__restrict enableColorEffectPtr,
											const u8 *__restrict sprAlphaPtr,
											const u8 *__restrict sprModePtr);
#endif
	
	template<bool ISDEBUGRENDER, bool ISOBJMODEBITMAP> FORCEINLINE void _RenderSpriteUpdatePixel(GPUEngineCompositorInfo &compInfo, size_t frameX, const u16 *__restrict srcPalette, const u8 palIndex, const OBJMode objMode, const u8 prio, const u8 spriteNum, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab);
	template<bool ISDEBUGRENDER> void _RenderSpriteBMP(GPUEngineCompositorInfo &compInfo, const u32 objAddress, const size_t length, size_t frameX, size_t spriteX, const s32 readXStep, const u8 spriteAlpha, const OBJMode objMode, const u8 prio, const u8 spriteNum, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab);
	template<bool ISDEBUGRENDER> void _RenderSprite256(GPUEngineCompositorInfo &compInfo, const u32 objAddress, const size_t length, size_t frameX, size_t spriteX, const s32 readXStep, const u16 *__restrict palColorBuffer, const OBJMode objMode, const u8 prio, const u8 spriteNum, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab);
	template<bool ISDEBUGRENDER> void _RenderSprite16(GPUEngineCompositorInfo &compInfo, const u32 objAddress, const size_t length, size_t frameX, size_t spriteX, const s32 readXStep, const u16 *__restrict palColorBuffer, const OBJMode objMode, const u8 prio, const u8 spriteNum, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab);
	void _RenderSpriteWin(const u8 *src, const bool col256, const size_t lg, size_t sprX, size_t x, const s32 xdir);
	bool _ComputeSpriteVars(GPUEngineCompositorInfo &compInfo, const OAMAttributes &spriteInfo, SpriteSize &sprSize, s32 &sprX, s32 &sprY, s32 &x, s32 &y, s32 &lg, s32 &xdir);
	
	u32 _SpriteAddressBMP(GPUEngineCompositorInfo &compInfo, const OAMAttributes &spriteInfo, const SpriteSize sprSize, const s32 y);
	
	template<bool ISDEBUGRENDER> void _SpriteRender(GPUEngineCompositorInfo &compInfo, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab);
	template<SpriteRenderMode MODE, bool ISDEBUGRENDER> void _SpriteRenderPerform(GPUEngineCompositorInfo &compInfo, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab);
	
public:
	GPUEngineBase();
	virtual ~GPUEngineBase();
	
	virtual void Reset();
	
	void SetupBuffers();
	void SetupRenderStates(void *nativeBuffer, void *customBuffer);
	template<NDSColorFormat OUTPUTFORMAT> void UpdateRenderStates(const size_t l);
	template<NDSColorFormat OUTPUTFORMAT> void RenderLine(const size_t l);
	
	void RefreshAffineStartRegs();
	
	void ParseReg_DISPCNT();
	void ParseReg_BGnCNT(const GPULayerID layerID);
	template<GPULayerID LAYERID> void ParseReg_BGnHOFS();
	template<GPULayerID LAYERID> void ParseReg_BGnVOFS();
	template<GPULayerID LAYERID> void ParseReg_BGnX();
	template<GPULayerID LAYERID> void ParseReg_BGnY();
	template<size_t WINNUM> void ParseReg_WINnH();
	void ParseReg_WININ();
	void ParseReg_WINOUT();
	void ParseReg_MOSAIC();
	void ParseReg_BLDCNT();
	void ParseReg_BLDALPHA();
	void ParseReg_BLDY();
	void ParseReg_MASTER_BRIGHT();
	
	void ParseAllRegisters();
	
	void UpdatePropertiesWithoutRender(const u16 l);
	void LastLineProcess();
	
	u32 vramBlockOBJAddress;
	
	size_t nativeLineRenderCount;
	size_t nativeLineOutputCount;
	bool isLineRenderNative[GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	bool isLineOutputNative[GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	
	IOREG_BG2X savedBG2X;
	IOREG_BG2Y savedBG2Y;
	IOREG_BG3X savedBG3X;
	IOREG_BG3Y savedBG3Y;
	
	const GPU_IOREG& GetIORegisterMap() const;
	
	bool IsMasterBrightFullIntensity() const;
	bool IsMasterBrightMaxOrMin() const;
	bool IsMasterBrightFullIntensityAtLineZero() const;
	void GetMasterBrightnessAtLineZero(GPUMasterBrightMode &outMode, u8 &outIntensity);
	
	bool GetEnableState();
	bool GetEnableStateApplied();
	void SetEnableState(bool theState);
	bool GetLayerEnableState(const size_t layerIndex);
	void SetLayerEnableState(const size_t layerIndex, bool theState);
	
	void ApplySettings();
	
	template<NDSColorFormat OUTPUTFORMAT> void RenderLineClearAsync();
	template<NDSColorFormat OUTPUTFORMAT> void RenderLineClearAsyncStart(bool willClearInternalCustomBuffer,
																		 s32 startLineIndex,
																		 u16 clearColor16,
																		 FragmentColor clearColor32);
	void RenderLineClearAsyncFinish();
	void RenderLineClearAsyncWaitForCustomLine(const s32 l);
	
	void UpdateMasterBrightnessDisplayInfo(NDSDisplayInfo &mutableInfo);
	template<NDSColorFormat OUTPUTFORMAT> void ApplyMasterBrightness(const NDSDisplayInfo &displayInfo);
	template<NDSColorFormat OUTPUTFORMAT, bool ISFULLINTENSITYHINT> void ApplyMasterBrightness(void *dst, const size_t pixCount, const GPUMasterBrightMode mode, const u8 intensity);
	
	const BGLayerInfo& GetBGLayerInfoByID(const GPULayerID layerID);
	
	void SpriteRenderDebug(const u16 lineIndex, u16 *dst);
	void RenderLayerBG(const GPULayerID layerID, u16 *dstLineColor);

	NDSDisplayID GetTargetDisplayByID() const;
	void SetTargetDisplayByID(const NDSDisplayID theDisplayID);
	
	GPUEngineID GetEngineID() const;
	
	void* GetRenderedBuffer() const;
	size_t GetRenderedWidth() const;
	size_t GetRenderedHeight() const;
	
	virtual void SetCustomFramebufferSize(size_t w, size_t h);
	template<NDSColorFormat OUTPUTFORMAT> void ResolveCustomRendering();
	void ResolveToCustomFramebuffer(NDSDisplayInfo &mutableInfo);
	
	void REG_DISPx_pack_test();
};

class GPUEngineA : public GPUEngineBase
{
private:
	GPUEngineA();
	~GPUEngineA();
	
protected:
	CACHE_ALIGN u16 _fifoLine16[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	CACHE_ALIGN FragmentColor _fifoLine32[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	
	CACHE_ALIGN u16 _VRAMNativeBlockCaptureCopy[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES * 4];
	u16 *_VRAMNativeBlockCaptureCopyPtr[4];
	
	FragmentColor *_3DFramebufferMain;
	u16 *_3DFramebuffer16;
	
	u16 *_VRAMNativeBlockPtr[4];
	void *_VRAMCustomBlockPtr[4];
	
	size_t _nativeLineCaptureCount[4];
	bool _isLineCaptureNative[4][GPU_VRAM_BLOCK_LINES];
	
	u16 *_captureWorkingDisplay16;
	u16 *_captureWorkingA16;
	u16 *_captureWorkingB16;
	FragmentColor *_captureWorkingA32;
	FragmentColor *_captureWorkingB32;
	
	DISPCAPCNT_parsed _dispCapCnt;
	bool _displayCaptureEnable;
	
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING> void _LineLarge8bpp(GPUEngineCompositorInfo &compInfo);
	
	template<NDSColorFormat OUTPUTFORMAT, size_t CAPTURELENGTH>
	void _RenderLine_DisplayCaptureCustom(const IOREG_DISPCAPCNT &DISPCAPCNT,
										  const GPUEngineLineInfo &lineInfo,
										  const bool isReadDisplayLineNative,
										  const bool isReadVRAMLineNative,
										  const void *srcAPtr,
										  const void *srcBPtr,
										  void *dstCustomPtr);  // Do not use restrict pointers, since srcB and dst can be the same
	
	template<NDSColorFormat OUTPUTFORMAT, size_t CAPTURELENGTH> void _RenderLine_DisplayCapture(const GPUEngineCompositorInfo &compInfo);
	void _RenderLine_DispCapture_FIFOToBuffer(u16 *fifoLineBuffer);
	
	template<NDSColorFormat COLORFORMAT, int SOURCESWITCH, size_t CAPTURELENGTH, bool CAPTUREFROMNATIVESRC, bool CAPTURETONATIVEDST>
	void _RenderLine_DispCapture_Copy(const GPUEngineLineInfo &lineInfo, const void *src, void *dst, const size_t captureLengthExt); // Do not use restrict pointers, since src and dst can be the same
	
	u16 _RenderLine_DispCapture_BlendFunc(const u16 srcA, const u16 srcB, const u8 blendEVA, const u8 blendEVB);
	template<NDSColorFormat COLORFORMAT> FragmentColor _RenderLine_DispCapture_BlendFunc(const FragmentColor srcA, const FragmentColor srcB, const u8 blendEVA, const u8 blendEVB);
	
#ifdef ENABLE_SSE2
	template<NDSColorFormat COLORFORMAT> __m128i _RenderLine_DispCapture_BlendFunc_SSE2(const __m128i &srcA, const __m128i &srcB, const __m128i &blendEVA, const __m128i &blendEVB);
#endif
	
	template<NDSColorFormat OUTPUTFORMAT>
	void _RenderLine_DispCapture_BlendToCustomDstBuffer(const void *srcA, const void *srcB, void *dst, const u8 blendEVA, const u8 blendEVB, const size_t length); // Do not use restrict pointers, since srcB and dst can be the same
	
	template<NDSColorFormat OUTPUTFORMAT, size_t CAPTURELENGTH, bool CAPTUREFROMNATIVESRCA, bool CAPTUREFROMNATIVESRCB, bool CAPTURETONATIVEDST>
	void _RenderLine_DispCapture_Blend(const GPUEngineLineInfo &lineInfo, const void *srcA, const void *srcB, void *dst, const size_t captureLengthExt); // Do not use restrict pointers, since srcB and dst can be the same
	
	template<NDSColorFormat OUTPUTFORMAT> void _HandleDisplayModeVRAM(const GPUEngineLineInfo &lineInfo);
	template<NDSColorFormat OUTPUTFORMAT> void _HandleDisplayModeMainMemory(const GPUEngineLineInfo &lineInfo);
	
public:
	static GPUEngineA* Allocate();
	void FinalizeAndDeallocate();
	
	void ParseReg_DISPCAPCNT();
	bool IsLineCaptureNative(const size_t blockID, const size_t blockLine);
	void* GetCustomVRAMBlockPtr(const size_t blockID);
	FragmentColor* Get3DFramebufferMain() const;
	u16* Get3DFramebuffer16() const;
	virtual void SetCustomFramebufferSize(size_t w, size_t h);
	
	bool WillRender3DLayer();
	bool WillCapture3DLayerDirect(const size_t l);
	bool WillDisplayCapture(const size_t l);
	void SetDisplayCaptureEnable();
	void ResetDisplayCaptureEnable();
	bool VerifyVRAMLineDidChange(const size_t blockID, const size_t l);
	
	void LastLineProcess();
	
	virtual void Reset();
	void ResetCaptureLineStates(const size_t blockID);
	
	template<NDSColorFormat OUTPUTFORMAT> void RenderLine(const size_t l);
	template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST> void RenderLine_Layer3D(GPUEngineCompositorInfo &compInfo);
};

class GPUEngineB : public GPUEngineBase
{
private:
	GPUEngineB();
	~GPUEngineB();
	
public:
	static GPUEngineB* Allocate();
	void FinalizeAndDeallocate();
	
	virtual void Reset();
	
	template<NDSColorFormat OUTPUTFORMAT> void RenderLine(const size_t l);
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
	virtual void DidFrameBegin(const size_t line, const bool isFrameSkipRequested, const size_t pageCount, u8 &selectedBufferIndexInOut) = 0;
	virtual void DidFrameEnd(bool isFrameSkipped, const NDSDisplayInfo &latestDisplayInfo) = 0;
	virtual void DidRender3DBegin() = 0;
	virtual void DidRender3DEnd() = 0;
	virtual void DidApplyGPUSettingsBegin() = 0;
	virtual void DidApplyGPUSettingsEnd() = 0;
	virtual void DidApplyRender3DSettingsBegin() = 0;
	virtual void DidApplyRender3DSettingsEnd() = 0;
};

// All of the default event handler methods should do nothing.
// If a subclass doesn't need to override every method, then it might be easier
// if you subclass GPUEventHandlerDefault instead of GPUEventHandler.
class GPUEventHandlerDefault : public GPUEventHandler
{
public:
	virtual void DidFrameBegin(const size_t line, const bool isFrameSkipRequested, const size_t pageCount, u8 &selectedBufferIndexInOut);
	virtual void DidFrameEnd(bool isFrameSkipped, const NDSDisplayInfo &latestDisplayInfo) {};
	virtual void DidRender3DBegin() {};
	virtual void DidRender3DEnd() {};
	virtual void DidApplyGPUSettingsBegin() {};
	virtual void DidApplyGPUSettingsEnd() {};
	virtual void DidApplyRender3DSettingsBegin() {};
	virtual void DidApplyRender3DSettingsEnd() {};
};

class GPUSubsystem
{
private:
	GPUEventHandlerDefault *_defaultEventHandler;
	GPUEventHandler *_event;
	
	GPUEngineA *_engineMain;
	GPUEngineB *_engineSub;
	NDSDisplay *_display[2];
	float _backlightIntensityTotal[2];
	GPUEngineLineInfo _lineInfo[GPU_VRAM_BLOCK_LINES + 1];
	
	Task *_asyncEngineBufferSetupTask;
	bool _asyncEngineBufferSetupIsRunning;
	
	int _pending3DRendererID;
	bool _needChange3DRenderer;
	
	u32 _videoFrameIndex;			// Increments whenever a video frame is completed. Resets every 60 video frames.
	u32 _render3DFrameCount;		// The current 3D rendering frame count, saved to this variable once every 60 video frames.
	bool _frameNeedsFinish;
	bool _willFrameSkip;
	bool _willPostprocessDisplays;
	bool _willAutoResolveToCustomBuffer;
	void *_customVRAM;
	void *_customVRAMBlank;
	
	void *_masterFramebuffer;
	
	NDSDisplayInfo _displayInfo;
	
	void _UpdateFPSRender3D();
	void _AllocateFramebuffers(NDSColorFormat outputFormat, size_t w, size_t h, size_t pageCount);
	
	u8* _DownscaleAndConvertForSavestate(const NDSDisplayID displayID, void *__restrict intermediateBuffer);
	
public:
	GPUSubsystem();
	~GPUSubsystem();
	
	void SetEventHandler(GPUEventHandler *eventHandler);
	GPUEventHandler* GetEventHandler();
	
	void Reset();
	void ForceRender3DFinishAndFlush(bool willFlush);
	void ForceFrameStop();
	
	const NDSDisplayInfo& GetDisplayInfo(); // Frontends need to call this whenever they need to read the video buffers from the emulator core
	const GPUEngineLineInfo& GetLineInfoAtIndex(size_t l);
	u32 GetFPSRender3D() const;
	
	GPUEngineA* GetEngineMain();
	GPUEngineB* GetEngineSub();
	NDSDisplay* GetDisplayMain();
	NDSDisplay* GetDisplayTouch();
	
	void* GetCustomVRAMBuffer();
	void* GetCustomVRAMBlankBuffer();
	template<NDSColorFormat COLORFORMAT> void* GetCustomVRAMAddressUsingMappedAddress(const u32 addr, const size_t offset);
	
	size_t GetFramebufferPageCount() const;
	void SetFramebufferPageCount(size_t pageCount);
	
	size_t GetCustomFramebufferWidth() const;
	size_t GetCustomFramebufferHeight() const;
	void SetCustomFramebufferSize(size_t w, size_t h);
	
	NDSColorFormat GetColorFormat() const;
	void SetColorFormat(const NDSColorFormat outputFormat);
	
	int Get3DRendererID();
	void Set3DRendererByID(int rendererID);
	bool Change3DRendererByID(int rendererID);
	bool Change3DRendererIfNeeded();
	
	bool GetWillFrameSkip() const;
	void SetWillFrameSkip(const bool willFrameSkip);
	void SetDisplayCaptureEnable();
	void ResetDisplayCaptureEnable();
	void UpdateRenderProperties();
	
	// By default, the displays will automatically perform certain postprocessing steps on the
	// CPU before the DidFrameEnd event.
	//
	// To turn off this behavior, call SetWillPostprocessDisplays() and pass a value of "false".
	// This can be useful if the client wants to perform these postprocessing steps itself, for
	// example, if a client performs them on another thread or on the GPU.
	//
	// If automatic postprocessing is turned off, clients can still manually perform the
	// postprocessing steps on the CPU by calling PostprocessDisplay().
	//
	// The postprocessing steps that are performed are:
	// - Converting an RGB666 formatted framebuffer to RGB888 format.
	// - Applying the master brightness.
	bool GetWillPostprocessDisplays() const;
	void SetWillPostprocessDisplays(const bool willPostprocess);
	void PostprocessDisplay(const NDSDisplayID displayID, NDSDisplayInfo &mutableInfo);
	
	// Normally, the GPUs will automatically resolve their native buffers to the master
	// custom framebuffer at the end of V-blank so that all rendered graphics are contained
	// within a single common buffer. This is necessary for when someone wants to read
	// the NDS framebuffers, but the reader can only read a single buffer at a time.
	// Certain functions, such as taking screenshots, as well as many frontends running
	// the NDS video displays, require that they read from only a single buffer.
	//
	// However, if SetWillAutoResolveToCustomBuffer() is passed "false", then the frontend
	// becomes responsible for calling GetDisplayInfo() and reading the native and custom buffers
	// properly for each display. If a single buffer is still needed for certain cases, then the
	// frontend must manually call ResolveDisplayToCustomFramebuffer() for each display before
	// reading the master custom framebuffer.
	bool GetWillAutoResolveToCustomBuffer() const;
	void SetWillAutoResolveToCustomBuffer(const bool willAutoResolve);
	void ResolveDisplayToCustomFramebuffer(const NDSDisplayID displayID, NDSDisplayInfo &mutableInfo);
	
	void SetupEngineBuffers();
	void AsyncSetupEngineBuffersStart();
	void AsyncSetupEngineBuffersFinish();
	
	template<NDSColorFormat OUTPUTFORMAT> void RenderLine(const size_t l);
	void UpdateAverageBacklightIntensityTotal();
	void ClearWithColor(const u16 colorBGRA5551);
	
	void SaveState(EMUFILE &os);
	bool LoadState(EMUFILE &is, int size);
};

class GPUClientFetchObject
{
protected:
	NDSDisplayInfo _fetchDisplayInfo[MAX_FRAMEBUFFER_PAGES];
	volatile u8 _lastFetchIndex;
	void *_clientData;
	
	virtual void _FetchNativeDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex);
	virtual void _FetchCustomDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex);
	
public:
	GPUClientFetchObject();
	virtual ~GPUClientFetchObject() {};
	
	virtual void Init();
	virtual void SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo);
	virtual void FetchFromBufferIndex(const u8 index);
	
	u8 GetLastFetchIndex() const;
	void SetLastFetchIndex(const u8 fetchIndex);
	
	const NDSDisplayInfo& GetFetchDisplayInfoForBufferIndex(const u8 bufferIndex) const;
	void SetFetchDisplayInfo(const NDSDisplayInfo &displayInfo);
	
	void* GetClientData() const;
	void SetClientData(void *clientData);
};

template <s32 INTEGERSCALEHINT, bool SCALEVERTICAL, bool USELINEINDEX, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
void CopyLineExpandHinted(const void *__restrict srcBuffer, const size_t srcLineIndex,
						  void *__restrict dstBuffer, const size_t dstLineIndex, const size_t dstLineWidth, const size_t dstLineCount);

template <s32 INTEGERSCALEHINT, bool SCALEVERTICAL, bool USELINEINDEX, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
void CopyLineExpandHinted(const GPUEngineLineInfo &lineInfo, const void *__restrict srcBuffer, void *__restrict dstBuffer);

template <s32 INTEGERSCALEHINT, bool USELINEINDEX, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
void CopyLineReduceHinted(const void *__restrict srcBuffer, const size_t srcLineIndex, const size_t srcLineWidth,
						  void *__restrict dstBuffer, const size_t dstLineIndex);

template <s32 INTEGERSCALEHINT, bool USELINEINDEX, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
void CopyLineReduceHinted(const GPUEngineLineInfo &lineInfo, const void *__restrict srcBuffer, void *__restrict dstBuffer);

extern GPUSubsystem *GPU;
extern MMU_struct MMU;

#endif
