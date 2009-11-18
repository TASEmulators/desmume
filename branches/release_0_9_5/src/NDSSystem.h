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
#include "emufile.h"

#include <string>

#ifdef WIN32
#include "pathsettings.h"
#endif

template<typename Type>
struct buttonstruct {
	union {
		struct {
			// changing the order of these fields would break stuff
			//fRLDUTSBAYXWEg
			Type G; // debug
			Type E; // right shoulder
			Type W; // left shoulder
			Type X;
			Type Y;
			Type A;
			Type B;
			Type S; // start
			Type T; // select
			Type U; // up
			Type D; // down
			Type L; // left
			Type R; // right
			Type F; // lid
		};
		Type array[14];
	};
};

extern buttonstruct<bool> Turbo;
extern buttonstruct<int> TurboTime;
extern buttonstruct<bool> AutoHold;

int NDS_WritePNG(const char *fname);

extern volatile bool execute;
extern BOOL click;

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

extern u64 nds_timer;
void NDS_Reschedule();
void NDS_RescheduleGXFIFO(u32 cost);
void NDS_RescheduleDMA();
void NDS_RescheduleTimers();

typedef struct
{
	s32 wifiCycle;
	s32 cycles;
	u64 timerCycle[2][4];
	u32 VCount;
	u32 old;

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
	s32 cpuloopIterationCount; //counts the number of times during a frame that a reschedule happened

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

void Desmume_InitOnce();

void NDS_DeInit(void);
void
NDS_FillDefaultFirmwareConfigData( struct NDS_fw_config_data *fw_config);

BOOL NDS_SetROM(u8 * rom, u32 mask);
NDS_header * NDS_getROMHeader(void);

struct GameInfo
{
	GameInfo()
		: romdata(NULL)
	{}

	void loadData(char* buf, int size)
	{
		resize(size);
		memcpy(romdata,buf,size);
	}

	void resize(int size) {
		if(romdata != NULL) delete[] romdata;
		romdata = new char[size];
		romsize = size;
	}
	u32 crc;
	NDS_header header;
	char ROMserial[20];
	void populate();
	char* romdata;
	int romsize;
};

typedef struct TSCalInfo
{
	struct adc
	{
		u16 x1, x2;
		u16 y1, y2;
	} adc;

	struct scr
	{
		u8 x1, x2;
		u8 y1, y2;
	} scr;

} TSCalInfo;

extern GameInfo gameInfo;


struct UserButtons : buttonstruct<bool>
{
};
struct UserTouch
{
	u16 touchX;
	u16 touchY;
	bool isTouch;
};
struct UserMicrophone
{
	u32 micButtonPressed;
};
struct UserInput
{
	UserButtons buttons;
	UserTouch touch;
	UserMicrophone mic;
};

// set physical user input
// these functions merely request the input to be changed.
// the actual change happens later at a specific time during the frame.
// this is to minimize the risk of desyncs.
void NDS_setTouchPos(u16 x, u16 y);
void NDS_releaseTouch(void);
void NDS_setPad(bool right,bool left,bool down,bool up,bool select,bool start,bool B,bool A,bool Y,bool X,bool leftShoulder,bool rightShoulder,bool debug, bool lid);
void NDS_setMic(bool pressed);

// get physical user input
// not including the results of autofire/etc.
// the effects of calls to "set physical user input" functions will be immediately reflected here though.
const UserInput& NDS_getRawUserInput();
const UserInput& NDS_getPrevRawUserInput();

// get final (fully processed) user input
// this should match whatever was or would be sent to the game
const UserInput& NDS_getFinalUserInput();

// set/get to-be-processed or in-the-middle-of-being-processed user input
// to process input, simply call this function and edit the return value.
// (applying autofire is one example of processing the input.)
// (movie playback is another example.)
// this must be done after the raw user input is set
// and before that input is sent to the game's memory.
UserInput& NDS_getProcessingUserInput();
bool NDS_isProcessingUserInput();
// call once per frame to prepare input for processing 
void NDS_beginProcessingInput();
// call once per frame to copy the processed input to the final input
void NDS_endProcessingInput();

// this is in case something needs reentrancy while processing input
void NDS_suspendProcessingInput(bool suspend);



int NDS_LoadROM(const char *filename, const char* logicalFilename=0);
void NDS_FreeROM(void);
void NDS_Reset();
int NDS_ImportSave(const char *filename);
bool NDS_ExportSave(const char *filename);

void nds_savestate(EMUFILE* os);
bool nds_loadstate(EMUFILE* is, int size);

int NDS_WriteBMP(const char *filename);
int NDS_LoadFirmware(const char *filename);
int NDS_CreateDummyFirmware( struct NDS_fw_config_data *user_settings);

void NDS_Sleep();

void NDS_SkipNextFrame();
#define NDS_SkipFrame(s) if(s) NDS_SkipNext2DFrame();
void NDS_OmitFrameSkip(int force=0);

void execHardware_doAllDma(EDMAMode modeNum);

template<bool FORCE> void NDS_exec(s32 nb = 560190<<1);

extern int lagframecounter;

static INLINE void NDS_ARM9HBlankInt(void)
{
    if(T1ReadWord(MMU.ARM9_REG, 4) & 0x10)
    {
         //MMU.reg_IF[0] |= 2;// & (MMU.reg_IME[0] << 1);// (MMU.reg_IE[0] & (1<<1));
		setIF(0, 2);
    }
}

static INLINE void NDS_ARM7HBlankInt(void)
{
    if(T1ReadWord(MMU.ARM7_REG, 4) & 0x10)
    {
        // MMU.reg_IF[1] |= 2;// & (MMU.reg_IME[1] << 1);// (MMU.reg_IE[1] & (1<<1));
		setIF(1, 2);
    }
}

static INLINE void NDS_ARM9VBlankInt(void)
{
    if(T1ReadWord(MMU.ARM9_REG, 4) & 0x8)
    {
        // MMU.reg_IF[0] |= 1;// & (MMU.reg_IME[0]);// (MMU.reg_IE[0] & 1);
		setIF(0, 1);
              //emu_halt();
              /*logcount++;*/
    }
}

static INLINE void NDS_ARM7VBlankInt(void)
{
    if(T1ReadWord(MMU.ARM7_REG, 4) & 0x8)
        // MMU.reg_IF[1] |= 1;// & (MMU.reg_IME[1]);// (MMU.reg_IE[1] & 1);
		setIF(1, 1);
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
		: GFX3D_HighResolutionInterpolateColor(true)
		, GFX3D_EdgeMark(true)
		, GFX3D_Fog(true)
		, UseExtBIOS(false)
		, SWIFromBIOS(false)
		, UseExtFirmware(false)
		, BootFromFirmware(false)
		, DebugConsole(false)
		//, gfx3d_flushMode(0)
		, num_cores(1)
		, micMode(InternalNoise)
		, spuInterpolationMode(SPUInterpolation_Linear)
		, manualBackupType(0)
	{
		strcpy(ARM9BIOS, "biosnds9.bin");
		strcpy(ARM7BIOS, "biosnds7.bin");
		strcpy(Firmware, "firmware.bin");

		wifi.mode = 0;
		wifi.infraBridgeAdapter = 0;

		for(int i=0;i<16;i++)
			spu_muteChannels[i] = false;
	}
	bool GFX3D_HighResolutionInterpolateColor;
	bool GFX3D_EdgeMark;
	bool GFX3D_Fog;

	bool UseExtBIOS;
	char ARM9BIOS[256];
	char ARM7BIOS[256];
	bool SWIFromBIOS;

	bool UseExtFirmware;
	char Firmware[256];
	bool BootFromFirmware;

	bool DebugConsole;

	int num_cores;
	bool single_core() { return num_cores==1; }
	
	struct _Wifi {
		int mode;
		int infraBridgeAdapter;
	} wifi;

	enum MicMode
	{
		InternalNoise = 0,
		Sample = 1,
		Random = 2,
		Physical = 3,
	} micMode;


	SPUInterpolationMode spuInterpolationMode;

	//this is a temporary hack until we straighten out the flushing logic and/or gxfifo
	//int gfx3d_flushMode;

	//this is the user's choice of manual backup type, for cases when the autodetection can't be trusted
	int manualBackupType;

	bool spu_muteChannels[16];

	struct _ShowGpu {
		_ShowGpu() : main(true), sub(true) {}
		union {
			struct { bool main,sub; };
			bool screens[2];
		};
	} showGpu;

	struct _Hud {
		_Hud() 
			: ShowInputDisplay(false)
			, ShowGraphicalInputDisplay(false)
			, FpsDisplay(false)
			, FrameCounterDisplay(false)
			, ShowLagFrameCounter(false)
			, ShowMicrophone(false)
		{}
		bool ShowInputDisplay, ShowGraphicalInputDisplay, FpsDisplay, FrameCounterDisplay, ShowLagFrameCounter, ShowMicrophone;
	} hud;

} CommonSettings;


extern std::string InputDisplayString;
extern int LagFrameFlag;
extern int lastLag, TotalLagFrames;

void MovieSRAM();

void ClearAutoHold(void);

#endif

 	  	 
