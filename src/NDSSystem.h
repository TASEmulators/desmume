/*  Copyright (C) 2006 yopyop
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

#ifndef NDSSYSTEM_H
#define NDSSYSTEM_H

#include <string.h>
#include "armcpu.h"
#include "MMU.h"
#include "driver.h"
#include "GPU.h"
#include "SPU.h"
#include "mem.h"
#include "wifi.h"

#include <string>

struct turbo {
	bool Right;
	bool Left;
	bool Down;
	bool Up;
	bool Select;
	bool Start;
	bool B;
	bool A;
	bool Y;
	bool X;
	bool L;
	bool R;

	bool &button(int i) { return ((bool*)this)[i]; }
};

extern turbo Turbo;

struct turbotime {
	int Right;
	int Left;
	int Down;
	int Up;
	int Select;
	int Start;
	int B;
	int A;
	int Y;
	int X;
	int L;
	int R;

	int &time(int i) { return ((int*)this)[i]; }
};

extern turbotime TurboTime;

struct autohold {
	bool Right;
	bool Left;
	bool Down;
	bool Up;
	bool Select;
	bool Start;
	bool B;
	bool A;
	bool Y;
	bool X;
	bool L;
	bool R;

	bool &hold(int i) { return ((bool*)this)[i]; }
};

extern autohold AutoHold;

int NDS_WritePNG(const char *fname);

extern volatile BOOL execute;
extern BOOL click;
extern char pathToROM[MAX_PATH];
extern char pathFilenameToROMwithoutExt[MAX_PATH];

/*
 * The firmware language values
 */
#define NDS_FW_LANG_JAP 0
#define NDS_FW_LANG_ENG 1
#define NDS_FW_LANG_FRE 2
#define NDS_FW_LANG_GER 3
#define NDS_FW_LANG_ITA 4
#define NDS_FW_LANG_SPA 5
#define NDS_FW_LANG_CHI 6
#define NDS_FW_LANG_RES 7


//#define LOG_ARM9
//#define LOG_ARM7

struct NDS_header
{
       char     gameTile[12];
       char     gameCode[4];
       u16      makerCode;
       u8       unitCode;
       u8       deviceCode;
       u8       cardSize;
       u8       cardInfo[8];
       u8       flags;
       
       u32      ARM9src;
       u32      ARM9exe;
       u32      ARM9cpy;
       u32      ARM9binSize;
       
       u32      ARM7src;
       u32      ARM7exe;
       u32      ARM7cpy;
       u32      ARM7binSize;
       
       u32      FNameTblOff;
       u32      FNameTblSize;
       
       u32      FATOff;
       u32      FATSize;
       
       u32     ARM9OverlayOff;
       u32     ARM9OverlaySize;
       u32     ARM7OverlayOff;
       u32     ARM7OverlaySize;
       
       u32     unknown2a;
       u32     unknown2b;
       
       u32     IconOff;
       u16     CRC16;
       u16     ROMtimeout;
       u32     ARM9unk;
       u32     ARM7unk;
       
       u8      unknown3c[8];
       u32     ROMSize;
       u32     HeaderSize;
       u8      unknown5[56];
       u8      logo[156];
       u16     logoCRC16;
       u16     headerCRC16;
       u8      reserved[160];
};

extern void debug();
void emu_halt();

typedef struct
{
	s32 ARM9Cycle;
	s32 ARM7Cycle;
	s32 wifiCycle;
	s32 cycles;
	s32 timerCycle[2][4];
	BOOL timerOver[2][4];
	s32 nextHBlank;
	u32 VCount;
	u32 old;
	s32 diff;
	BOOL lignerendu;

	u16 touchX;
	u16 touchY;
	BOOL isTouch;
	u16 pad;

	u8 *FW_ARM9BootCode;
	u8 *FW_ARM7BootCode;
	u32 FW_ARM9BootCodeAddr;
	u32 FW_ARM7BootCodeAddr;
	u32 FW_ARM9BootCodeSize;
	u32 FW_ARM7BootCodeSize;

	BOOL sleeping;

	//this is not essential NDS runtime state.
	//it was perhaps a mistake to put it here.
	//it is far less important than the above.
	//maybe I should move it.
	s32 idleCycles;
	s32 runCycleCollector[16];
	s32 idleFrameCounter;

	//if the game was booted on a debug console, this is set
	BOOL debugConsole;

	bool isInVblank() const { return VCount >= 192; } 
	bool isIn3dVblank() const { return VCount >= 192 && VCount<215; } 
} NDSSystem;

/** /brief A touchscreen calibration point.
 */
struct NDS_fw_touchscreen_cal {
  u16 adc_x;
  u16 adc_y;

  u8 screen_x;
  u8 screen_y;
};

/** /brief The type of DS
 */
enum nds_fw_ds_type {
  NDS_FW_DS_TYPE_FAT,
  NDS_FW_DS_TYPE_LITE,
  NDS_FW_DS_TYPE_iQue
};

#define MAX_FW_NICKNAME_LENGTH 10
#define MAX_FW_MESSAGE_LENGTH 26

struct NDS_fw_config_data {
  enum nds_fw_ds_type ds_type;

  u8 fav_colour;
  u8 birth_month;
  u8 birth_day;

  u16 nickname[MAX_FW_NICKNAME_LENGTH];
  u8 nickname_len;

  u16 message[MAX_FW_MESSAGE_LENGTH];
  u8 message_len;

  u8 language;

  /* touchscreen calibration */
  struct NDS_fw_touchscreen_cal touch_cal[2];
};

extern NDSSystem nds;

#ifdef GDB_STUB
int NDS_Init( struct armcpu_memory_iface *arm9_mem_if,
              struct armcpu_ctrl_iface **arm9_ctrl_iface,
              struct armcpu_memory_iface *arm7_mem_if,
              struct armcpu_ctrl_iface **arm7_ctrl_iface);
#else
int NDS_Init ( void);
#endif

void NDS_DeInit(void);
void
NDS_FillDefaultFirmwareConfigData( struct NDS_fw_config_data *fw_config);

BOOL NDS_SetROM(u8 * rom, u32 mask);
NDS_header * NDS_getROMHeader(void);

struct GameInfo
{
	u32 crc;
	NDS_header header;
	char ROMserial[20];
	void populate();
};

extern GameInfo gameInfo;
 
void NDS_setTouchPos(u16 x, u16 y);
void NDS_releaseTouch(void);
void NDS_setPad(bool R,bool L,bool D,bool U,bool T,bool S,bool B,bool A,bool Y,bool X,bool W,bool E,bool G, bool F);
void NDS_setPadFromMovie(u16 pad);

#ifdef EXPERIMENTAL_GBASLOT
int NDS_LoadROM(const char *filename);
#else
int NDS_LoadROM(const char *filename,
                 const char *cflash_disk_image_file);
#endif
void NDS_FreeROM(void);
void NDS_Reset();
int NDS_ImportSave(const char *filename);
bool NDS_ExportSave(const char *filename);

int NDS_WriteBMP(const char *filename);
int NDS_LoadFirmware(const char *filename);
int NDS_CreateDummyFirmware( struct NDS_fw_config_data *user_settings);

void NDS_Sleep();

void NDS_SkipNextFrame();
#define NDS_SkipFrame(s) if(s) NDS_SkipNext2DFrame();

template<bool FORCE> void NDS_exec(s32 nb = 560190<<1);

extern int lagframecounter;

       static INLINE void NDS_ARM9HBlankInt(void)
       {
            if(T1ReadWord(ARM9Mem.ARM9_REG, 4) & 0x10)
            {
                 //MMU.reg_IF[0] |= 2;// & (MMU.reg_IME[0] << 1);// (MMU.reg_IE[0] & (1<<1));
				setIF(0, 2);
                 NDS_ARM9.wIRQ = TRUE;
            }
       }
       
       static INLINE void NDS_ARM7HBlankInt(void)
       {
            if(T1ReadWord(MMU.ARM7_REG, 4) & 0x10)
            {
                // MMU.reg_IF[1] |= 2;// & (MMU.reg_IME[1] << 1);// (MMU.reg_IE[1] & (1<<1));
				setIF(1, 2);
                 NDS_ARM7.wIRQ = TRUE;
            }
       }
       
       static INLINE void NDS_ARM9VBlankInt(void)
       {
            if(T1ReadWord(ARM9Mem.ARM9_REG, 4) & 0x8)
            {
                // MMU.reg_IF[0] |= 1;// & (MMU.reg_IME[0]);// (MMU.reg_IE[0] & 1);
				setIF(0, 1);
                 NDS_ARM9.wIRQ = TRUE;
                      //emu_halt();
                      /*logcount++;*/
            }
       }
       
       static INLINE void NDS_ARM7VBlankInt(void)
       {
            if(T1ReadWord(MMU.ARM7_REG, 4) & 0x8)
                // MMU.reg_IF[1] |= 1;// & (MMU.reg_IME[1]);// (MMU.reg_IE[1] & 1);
				setIF(1, 1);
                 NDS_ARM7.wIRQ = TRUE;
                 //emu_halt();
       }
       
       static INLINE void NDS_swapScreen(void)
       {
	       u16 tmp = MainScreen.offset;
	       MainScreen.offset = SubScreen.offset;
	       SubScreen.offset = tmp;
       }

	int NDS_WriteBMP_32bppBuffer(int width, int height, const void* buf, const char *filename);

extern struct TCommonSettings {
	TCommonSettings() 
		: HighResolutionInterpolateColor(true)
		, UseExtBIOS(false)
		, SWIFromBIOS(false)
		, UseExtFirmware(false)
		, BootFromFirmware(false)
		, DebugConsole(false)
		, wifiBridgeAdapterNum(0)
		, spuInterpolationMode(SPUInterpolation_Linear)
		, spuAdpcmCache(false)
		, gfx3d_flushMode(0)
		, manualBackupType(0)
	{
		strcpy(ARM9BIOS, "biosnds9.bin");
		strcpy(ARM7BIOS, "biosnds7.bin");
		strcpy(Firmware, "firmware.bin");
	}
	bool HighResolutionInterpolateColor;

	bool UseExtBIOS;
	char ARM9BIOS[256];
	char ARM7BIOS[256];
	bool SWIFromBIOS;

	bool UseExtFirmware;
	char Firmware[256];
	bool BootFromFirmware;

	bool DebugConsole;
	
	int wifiBridgeAdapterNum;

	SPUInterpolationMode spuInterpolationMode;
	bool spuAdpcmCache;

	//this is a temporary hack until we straighten out the flushing logic and/or gxfifo
	int gfx3d_flushMode;

	//this is the user's choice of manual backup type, for cases when the autodetection can't be trusted
	int manualBackupType;

	struct _ShowGpu {
		_ShowGpu() : main(true), sub(true) {}
		union {
			struct { bool main,sub; };
			bool screens[2];
		};
	} showGpu;

} CommonSettings;


extern std::string InputDisplayString;
extern int LagFrameFlag;
extern int lastLag, TotalLagFrames;

void MovieSRAM();

void ClearAutoHold(void);

#endif

 	  	 
