/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2008-2013 DeSmuME team

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
#include "utils/task.h"

#include <string>

#if defined(HOST_WINDOWS)
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

extern CFIRMWARE	*firmware;

#define DSGBA_LOADER_SIZE 512
enum
{
	ROM_NDS = 0,
	ROM_DSGBA
};

//#define LOG_ARM9
//#define LOG_ARM7

#include "PACKED.h"
struct NDS_header
{
	char	gameTile[12];		// 000 - Game Title (uppercase ASCII, padded with 00h)
	char	gameCode[4];		// 00C - Gamecode (uppercase ASCII, NTR-<code>, 0=homebrew)
	u16		makerCode;			// 010 - Makercode (uppercase ASCII, 0=homebrew)
	u8		unitCode;			// 012 - Unitcode (00h=Nintendo DS)
	u8		deviceCode;			// 013 - Encryption Seed Select (00..07h, usually 00h)
	u8		cardSize;			// 014 - Devicecapacity (Chipsize = 128KB SHL nn) (eg. 7 = 16MB)
	u8		reserved1[8];		// 015 - Must be set to 0x00
	u8		region;				// 01D - Specific region: 0x80 - China, 0x40 - Korea, 0x00 - Other
	u8		romversion;			// 01E - ROM Version (usually 00h)
	u8		autostart;			// 01F - Autostart (Bit2: Skip "Press Button" after Health and Safety)
								//	 (Also skips bootmenu, even in Manual mode & even Start pressed)
	u32		ARM9src;			// 020 - ARM9 rom_offset    (4000h and up, align 1000h)
	u32		ARM9exe;			// 024 - ARM9 entry_address (2000000h..23BFE00h)
	u32		ARM9cpy;			// 028 - ARM9 ram_address   (2000000h..23BFE00h)
	u32		ARM9binSize;		// 02C - ARM9 size          (max 3BFE00h) (3839.5KB)

	u32		ARM7src;			// 030 - ARM7 rom_offset    (8000h and up)
	u32		ARM7exe;			// 034 - ARM7 entry_address (2000000h..23BFE00h, or 37F8000h..3807E00h)
	u32		ARM7cpy;			// 038 - ARM7 ram_address   (2000000h..23BFE00h, or 37F8000h..3807E00h)
	u32		ARM7binSize;		// 03C - ARM7 size          (max 3BFE00h, or FE00h) (3839.5KB, 63.5KB)

	u32		FNameTblOff;		// 040 - File Name Table (FNT) offset
	u32		FNameTblSize;		// 044 - File Name Table (FNT) size

	u32		FATOff;				// 048 - File Allocation Table (FAT) offset
	u32		FATSize;			// 04C - File Allocation Table (FAT) size

	u32		ARM9OverlayOff;		// 050 - File ARM9 overlay_offset
	u32		ARM9OverlaySize;	// 054 - File ARM9 overlay_size
	u32		ARM7OverlayOff;		// 058 - File ARM7 overlay_offset
	u32		ARM7OverlaySize;	// 05C - File ARM7 overlay_size

	u32		normalCmd;			// 060 - Port 40001A4h setting for normal commands (usually 00586000h)
	u32		Key1Cmd;			// 064 - Port 40001A4h setting for KEY1 commands   (usually 001808F8h)

	u32		IconOff;			// 068 - Icon_title_offset (0=None) (8000h and up)
	u16		CRC16;				// 06C - Secure Area Checksum, CRC-16 of [ [20h]..7FFFh] - Calculations with this algorithm use 0xffff as the initial value
	u16		ROMtimeout;			// 06E - Secure Area Loading Timeout (usually 051Eh)
	u32		ARM9autoload;		// 070 - ARM9 Auto Load List RAM Address
	u32		ARM7autoload;		// 074 - ARM7 Auto Load List RAM Address

	u8		infoResevedRegion[8];	// 078 - ROM Information Reserved Region (must be set to 0x00)
	u32		endROMoffset;		// 080 - Total Used ROM size (remaining/unused bytes usually FFh-padded)
	u32		HeaderSize;			// 084 - ROM Header Size (4000h)
	u32		ARM9module;			// 088 - ARM9 Module Parameter Address (auto-load parameters)
	u32		ARM7module;			// 08C - ARM7 Module Parameter Address (auto-load parameters)
	u8		reserved2[48];		// 090 - Must be set to 0x00 - "PASS" is contained within here?
	u8		logo[156];			// 0C0 - Nintendo Logo (compressed bitmap, same as in GBA Headers)
	u16		logoCRC16;			// 15C - Nintendo Logo Checksum, CRC-16 of [0C0h-15Bh], fixed CF56h
	u16		headerCRC16;		// 15E - Header Checksum, CRC-16 of [000h-15Dh]
	u8		reserved[160];		// Must be set to 0x00
};
#include "PACKED_END.h"

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

enum NDS_CONSOLE_TYPE
{
	NDS_CONSOLE_TYPE_FAT = 0xFF,
	NDS_CONSOLE_TYPE_LITE = 0x20,
	NDS_CONSOLE_TYPE_IQUE = 0x43,
	NDS_CONSOLE_TYPE_IQUE_LIE = 0x63,
	NDS_CONSOLE_TYPE_DSI = 0xFE
};

struct NDSSystem
{
	s32 wifiCycle;
	s32 cycles;
	u64 timerCycle[2][4];
	u32 VCount;
	u32 old;

	//raw adc touch coords for old NDS
	u16 adc_touchX;
	u16 adc_touchY;
	s32 adc_jitterctr;
	BOOL stylusJitter;

	//the DSI returns calibrated touch coords from its TSC (?), so we need to save these separately
	u16 scr_touchX;
	u16 scr_touchY;

	//whether the console is using our faked-bootup process
	BOOL isFakeBooted;
	
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
	u32 freezeBus;

	//this is not essential NDS runtime state.
	//it was perhaps a mistake to put it here.
	//it is far less important than the above.
	//maybe I should move it.
	s32 idleCycles[2];
	s32 runCycleCollector[2][16];
	s32 idleFrameCounter;
	s32 cpuloopIterationCount; //counts the number of times during a frame that a reschedule happened

	//console type must be copied in when the system boots. it can't be changed on the fly.
	int ConsoleType;
	bool Is_DSI() { return ConsoleType == NDS_CONSOLE_TYPE_DSI; }
	bool Is_DebugConsole() { return _DebugConsole!=0; }
	BOOL _DebugConsole;
	
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

#define MAX_FW_NICKNAME_LENGTH 10
#define MAX_FW_MESSAGE_LENGTH 26

struct NDS_fw_config_data
{
  NDS_CONSOLE_TYPE ds_type;

  u8 fav_colour;
  u8 birth_month;
  u8 birth_day;

  u16 nickname[MAX_FW_NICKNAME_LENGTH];
  u8 nickname_len;

  u16 message[MAX_FW_MESSAGE_LENGTH];
  u8 message_len;

  u8 language;

  //touchscreen calibration
  NDS_fw_touchscreen_cal touch_cal[2];
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
	RomBanner(bool defaultInit = true);
	u16 version; //Version  (0001h)
	u16 crc16; //CRC16 across entries 020h..83Fh
	u8 reserved[0x1C]; //Reserved (zero-filled)
	u8 bitmap[0x200]; //Icon Bitmap  (32x32 pix) (4x4 tiles, each 4x8 bytes, 4bit depth)
	u16 palette[0x10]; //Icon Palette (16 colors, 16bit, range 0000h-7FFFh) (Color 0 is transparent, so the 1st palette entry is ignored)
	enum { NUM_TITLES = 6 };
	union {
		struct {
			u16 title_jp[0x80]; //Title 0 Japanese (128 characters, 16bit Unicode)
			u16 title_en[0x80]; //Title 1 English  ("")
			u16 title_fr[0x80]; //Title 2 French   ("")
			u16 title_de[0x80]; //Title 3 German   ("")
			u16 title_it[0x80]; //Title 4 Italian  ("")
			u16 title_es[0x80]; //Title 5 Spanish  ("")
		};
		u16 titles[NUM_TITLES][0x80];
	};
	u8 end0xFF[0x1C0]; 
  //840h  ?    (Maybe newer/chinese firmware do also support chinese title?)
  //840h  -    End of Icon/Title structure (next 1C0h bytes usually FFh-filled)
};

struct GameInfo
{
	FILE *fROM;
	u8	*romdata;
	u32 romsize;
	u32 cardSize;
	u32 mask;
	u32 crc;
	u32 chipID;
	u32 lastReadPos;
	u32	romType;
	u32 headerOffset;
	char ROMserial[20];
	char ROMname[20];
	bool _isDSiEnhanced;
	NDS_header header;
	//a copy of the pristine secure area from the rom
	u8	secureArea[0x4000];
	RomBanner	banner;
	const RomBanner& getRomBanner();

	GameInfo() :	fROM(NULL),
					romdata(NULL),
					crc(0),
					chipID(0x00000FC2),
					romsize(0),
					cardSize(0),
					mask(0),
					lastReadPos(0xFFFFFFFF),
					romType(ROM_NDS),
					headerOffset(0),
					_isDSiEnhanced(false)
	{
		memset(&header, 0, sizeof(header));
		memset(&ROMserial[0], 0, sizeof(ROMserial));
		memset(&ROMname[0], 0, sizeof(ROMname));
	}

	~GameInfo() { closeROM(); }

	bool loadROM(std::string fname, u32 type = ROM_NDS);
	void closeROM();
	u32 readROM(u32 pos);
	void populate();
	bool isDSiEnhanced() { return _isDSiEnhanced; };
	bool isHomebrew() { return ((header.ARM9src < 0x4000) && (T1ReadLong(header.logo, 0) != 0x51AEFF24) && (T1ReadLong(header.logo, 4) != 0x699AA221)); }
	bool hasRomBanner();
	
};

typedef struct TSCalInfo
{
	struct
	{
		u16 x1, x2;
		u16 y1, y2;
		u16 width;
		u16 height;
	} adc;

	struct
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



int NDS_LoadROM(const char *filename, const char* physicalFilename=0, const char* logicalFilename=0);
void NDS_FreeROM(void);
void NDS_Reset();

bool NDS_LegitBoot();
bool NDS_FakeBoot();

void nds_savestate(EMUFILE* os);
bool nds_loadstate(EMUFILE* is, int size);

void NDS_Sleep();
void NDS_TriggerCardEjectIRQ();

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

extern struct TCommonSettings {
	TCommonSettings() 
		: GFX3D_HighResolutionInterpolateColor(true)
		, GFX3D_EdgeMark(true)
		, GFX3D_Fog(true)
		, GFX3D_Texture(true)
		, GFX3D_LineHack(true)
		, GFX3D_Zelda_Shadow_Depth_Hack(0)
		, GFX3D_Renderer_Multisample(false)
		, jit_max_block_size(100)
		, loadToMemory(false)
		, UseExtBIOS(false)
		, SWIFromBIOS(false)
		, PatchSWI3(false)
		, UseExtFirmware(false)
		, UseExtFirmwareSettings(false)
		, BootFromFirmware(false)
		, DebugConsole(false)
		, EnsataEmulation(false)
		, cheatsDisable(false)
		, rigorous_timing(false)
		, advanced_timing(true)
		, micMode(InternalNoise)
		, spuInterpolationMode(SPUInterpolation_Linear)
		, manualBackupType(0)
		, autodetectBackupMethod(0)
		, spu_captureMuted(false)
		, spu_advanced(false)
		, StylusPressure(50)
		, ConsoleType(NDS_CONSOLE_TYPE_FAT)
		, StylusJitter(false)
		, backupSave(false)
		, SPU_sync_mode(0)
		, SPU_sync_method(0)
	{
		strcpy(ARM9BIOS, "biosnds9.bin");
		strcpy(ARM7BIOS, "biosnds7.bin");
		strcpy(Firmware, "firmware.bin");
		NDS_FillDefaultFirmwareConfigData(&fw_config);

		/* WIFI mode: adhoc = 0, infrastructure = 1 */
		wifi.mode = 1;
		wifi.infraBridgeAdapter = 0;

		for(int i=0;i<16;i++)
			spu_muteChannels[i] = false;

		for(int g=0;g<2;g++)
			for(int x=0;x<5;x++)
				dispLayers[g][x]=true;
#ifdef HAVE_JIT
		//zero 06-sep-2012 - shouldnt be defaulting this to true for now, since the jit is buggy. 
		//id rather have people discover a bonus speedhack than discover new bugs in a new version
		use_jit = false;
#else
		use_jit = false;
#endif

		num_cores = getOnlineCores();
	}
	bool GFX3D_HighResolutionInterpolateColor;
	bool GFX3D_EdgeMark;
	bool GFX3D_Fog;
	bool GFX3D_Texture;
	bool GFX3D_LineHack;
	int  GFX3D_Zelda_Shadow_Depth_Hack;
	bool GFX3D_Renderer_Multisample;

	bool loadToMemory;

	bool UseExtBIOS;
	char ARM9BIOS[256];
	char ARM7BIOS[256];
	bool SWIFromBIOS;
	bool PatchSWI3;

	bool UseExtFirmware;
	bool UseExtFirmwareSettings;
	char Firmware[256];
	bool BootFromFirmware;
	NDS_fw_config_data fw_config;

	NDS_CONSOLE_TYPE ConsoleType;
	bool DebugConsole;
	bool EnsataEmulation;
	
	bool cheatsDisable;

	int num_cores;
	bool single_core() { return num_cores==1; }
	bool rigorous_timing;

	int StylusPressure;
	bool StylusJitter;

	bool dispLayers[2][5];
	
	FAST_ALIGN bool advanced_timing;

	bool use_jit;
	u32	jit_max_block_size;
	
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

	int autodetectBackupMethod;
	//this is the user's choice of manual backup type, for cases when the autodetection can't be trusted
	int manualBackupType;
	bool backupSave;

	int SPU_sync_mode;
	int SPU_sync_method;

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

//MUSTANG
//extern ADVANsCEne	advsc;

#endif

