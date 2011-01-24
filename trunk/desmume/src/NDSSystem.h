/*  Copyright (C) 2006 yopyop
    Copyright (C) 2008-2010 DeSmuME team

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
#include "firmware.h"
#include "types.h"

#include <string>

#if defined(_WINDOWS) && !defined(WXPORT)
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
	   u8		romversion;
       
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

enum ENSATA_HANDSHAKE
{
	ENSATA_HANDSHAKE_none = 0,
	ENSATA_HANDSHAKE_query = 1,
	ENSATA_HANDSHAKE_ack = 2,
	ENSATA_HANDSHAKE_confirm = 3,
	ENSATA_HANDSHAKE_complete = 4,
};

struct NDSSystem
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
	
	u16 paddle;

	u8 *FW_ARM9BootCode;
	u8 *FW_ARM7BootCode;
	u32 FW_ARM9BootCodeAddr;
	u32 FW_ARM7BootCodeAddr;
	u32 FW_ARM9BootCodeSize;
	u32 FW_ARM7BootCodeSize;

	BOOL sleeping;
	BOOL cardEjected;
	BOOL freezeBus;

	//this is not essential NDS runtime state.
	//it was perhaps a mistake to put it here.
	//it is far less important than the above.
	//maybe I should move it.
	s32 idleCycles[2];
	s32 runCycleCollector[2][16];
	s32 idleFrameCounter;
	s32 cpuloopIterationCount; //counts the number of times during a frame that a reschedule happened

	//if the game was booted on a debug console, this is set
	BOOL debugConsole;

	//set if the user requests ensata emulation
	BOOL ensataEmulation;

	//there is a hack in the ipc sync for ensata. this tracks its state
	u32 ensataIpcSyncCounter;

	//maintains the state of the ensata handshaking protocol
	u32 ensataHandshake;

	struct {
		u8 lcd, gpuMain, gfx3d_render, gfx3d_geometry, gpuSub, dispswap;
	} power1; //POWCNT1

	struct {
		u8 speakers, wifi /*(initial value=0)*/;
	} power2; //POWCNT2

	bool isInVblank() const { return VCount >= 192; } 
	bool isIn3dVblank() const { return VCount >= 192 && VCount<215; } 
};

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

BOOL NDS_SetROM(u8 * rom, u32 mask);
NDS_header * NDS_getROMHeader(void);

struct RomBanner
{
	RomBanner(bool defaultInit);
	u16 version; //Version  (0001h)
	u16 crc16; //CRC16 across entries 020h..83Fh
	u8 reserved[0x1C]; //Reserved (zero-filled)
	u8 bitmap[0x200]; //Icon Bitmap  (32x32 pix) (4x4 tiles, each 4x8 bytes, 4bit depth)
	u16 palette[0x10]; //Icon Palette (16 colors, 16bit, range 0000h-7FFFh) (Color 0 is transparent, so the 1st palette entry is ignored)
	enum { NUM_TITLES = 6 };
	union {
		struct {
			wchar_t title_jp[0x80]; //Title 0 Japanese (128 characters, 16bit Unicode)
			wchar_t title_en[0x80]; //Title 1 English  ("")
			wchar_t title_fr[0x80]; //Title 2 French   ("")
			wchar_t title_de[0x80]; //Title 3 German   ("")
			wchar_t title_it[0x80]; //Title 4 Italian  ("")
			wchar_t title_es[0x80]; //Title 5 Spanish  ("")
		};
		wchar_t titles[NUM_TITLES][0x80];
	};
	u8 end0xFF[0x1C0]; 
  //840h  ?    (Maybe newer/chinese firmware do also support chinese title?)
  //840h  -    End of Icon/Title structure (next 1C0h bytes usually FFh-filled)
};

struct GameInfo
{
	GameInfo()
		: romdata(NULL)
	{}

	void loadData(char* buf, int size)
	{
		resize(size);
		memcpy(romdata,buf,size);
		romsize = (u32)size;
		fillGap();
	}

	void fillGap()
	{
		memset(romdata+romsize,0xFF,allocatedSize-romsize);
	}

	void resize(int size) {
		if(romdata != NULL) delete[] romdata;

		//calculate the necessary mask for the requested size
		mask = size-1; 
		mask |= (mask >>1);
		mask |= (mask >>2);
		mask |= (mask >>4);
		mask |= (mask >>8);
		mask |= (mask >>16);

		//now, we actually need to over-allocate, because bytes from anywhere protected by that mask
		//could be read from the rom
		allocatedSize = mask+4;

		romdata = new char[allocatedSize];
		romsize = size;
	}
	u32 crc;
	NDS_header header;
	char ROMserial[20];
	char ROMname[20];
	//char ROMfullName[7][0x100];
	void populate();
	char* romdata;
	u32 romsize;
	u32 allocatedSize;
	u32 mask;
	const RomBanner& getRomBanner();
	bool hasRomBanner();
};

typedef struct TSCalInfo
{
	struct adc
	{
		u16 x1, x2;
		u16 y1, y2;
		u16 width;
		u16 height;
	} adc;

	struct scr
	{
		u8 x1, x2;
		u8 y1, y2;
		u16 width;
		u16 height;
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

void NDS_Sleep();
void NDS_ToggleCardEject();

void NDS_SkipNextFrame();
#define NDS_SkipFrame(s) if(s) NDS_SkipNext2DFrame();
void NDS_OmitFrameSkip(int force=0);

void NDS_debug_break();
void NDS_debug_continue();
void NDS_debug_step();

void execHardware_doAllDma(EDMAMode modeNum);

template<bool FORCE> void NDS_exec(s32 nb = 560190<<1);

extern int lagframecounter;

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
		, GFX3D_Texture(true)
		, GFX3D_Zelda_Shadow_Depth_Hack(0)
		, UseExtBIOS(false)
		, SWIFromBIOS(false)
		, PatchSWI3(false)
		, UseExtFirmware(false)
		, BootFromFirmware(false)
		, DebugConsole(false)
		, EnsataEmulation(false)
		, cheatsDisable(false)
		, num_cores(1)
		, rigorous_timing(false)
		, advanced_timing(true)
		, micMode(InternalNoise)
		, spuInterpolationMode(SPUInterpolation_Linear)
		, manualBackupType(0)
		, spu_captureMuted(false)
		, spu_advanced(false)
		, StylusPressure(50)
	{
		strcpy(ARM9BIOS, "biosnds9.bin");
		strcpy(ARM7BIOS, "biosnds7.bin");
		strcpy(Firmware, "firmware.bin");
		NDS_FillDefaultFirmwareConfigData(&InternalFirmConf);

		/* WIFI mode: adhoc = 0, infrastructure = 1 */
		wifi.mode = 1;
		wifi.infraBridgeAdapter = 0;

		for(int i=0;i<16;i++)
			spu_muteChannels[i] = false;

		for(int g=0;g<2;g++)
			for(int x=0;x<5;x++)
				dispLayers[g][x]=true;
	}
	bool GFX3D_HighResolutionInterpolateColor;
	bool GFX3D_EdgeMark;
	bool GFX3D_Fog;
	bool GFX3D_Texture;
	int  GFX3D_Zelda_Shadow_Depth_Hack;

	bool UseExtBIOS;
	char ARM9BIOS[256];
	char ARM7BIOS[256];
	bool SWIFromBIOS;
	bool PatchSWI3;

	bool UseExtFirmware;
	char Firmware[256];
	bool BootFromFirmware;
	struct NDS_fw_config_data InternalFirmConf;

	bool DebugConsole;
	bool EnsataEmulation;
	
	bool cheatsDisable;

	int num_cores;
	bool single_core() { return num_cores==1; }
	bool rigorous_timing;

	int StylusPressure;

	bool dispLayers[2][5];
	
	FAST_ALIGN bool advanced_timing;
	
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
	bool spu_captureMuted;
	bool spu_advanced;

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
			, ShowRTC(false)
		{}
		bool ShowInputDisplay, ShowGraphicalInputDisplay, FpsDisplay, FrameCounterDisplay, ShowLagFrameCounter, ShowMicrophone, ShowRTC;
	} hud;

} CommonSettings;


extern std::string InputDisplayString;
extern int LagFrameFlag;
extern int lastLag, TotalLagFrames;

void MovieSRAM();

void ClearAutoHold(void);

bool ValidateSlot2Access(u32 procnum, u32 demandSRAMSpeed, u32 demand1stROMSpeed, u32 demand2ndROMSpeed, int clockbits);

#endif

