/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2008-2021 DeSmuME team

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


#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <math.h>
#include <zlib.h>

#include <features/features_cpu.h>

#include "utils/decrypt/decrypt.h"
#include "utils/decrypt/crc.h"
#include "utils/decrypt/header.h"
#include "utils/advanscene.h"
#include "utils/task.h"
#include "utils/bits.h"

#include "common.h"
#include "armcpu.h"
#include "render3D.h"
#include "MMU.h"
#include "NDSSystem.h"
#include "gfx3d.h"
#include "GPU.h"
#include "cp15.h"
#include "bios.h"
#include "debug.h"
#include "cheatSystem.h"
#include "movie.h"
#include "FIFO.h"
#include "readwrite.h"
#include "registers.h"
#include "debug.h"
#include "driver.h"
#include "firmware.h"
#include "version.h"
#include "path.h"
#include "slot1.h"
#include "slot2.h"
#include "emufile.h"
#include "SPU.h"
#include "wifi.h"
#include "Database.h"
#include "frontend/modules/Disassembler.h"

#if defined(HOST_WINDOWS) && !defined(TARGET_INTERFACE)
#include "display.h"
extern HWND DisViewWnd[2];
#endif

#ifdef GDB_STUB
#include "gdbstub.h"
#define GDBSTUB_MUTEX_LOCK() gdbstub_mutex_lock()
#define GDBSTUB_MUTEX_UNLOCK() gdbstub_mutex_unlock()
#else
#define GDBSTUB_MUTEX_LOCK() do {} while(0)
#define GDBSTUB_MUTEX_UNLOCK() do {} while(0)
#endif

//int xxctr=0;
//#define LOG_ARM9
//#define LOG_ARM7
//#define dolog (currFrameCounter>30)
bool dolog = false;
//#define LOG_TO_FILE
//#define LOG_TO_FILE_REGS

//===============================================================
FILE *fp_dis7 = NULL;
FILE *fp_dis9 = NULL;

PathInfo path;

TCommonSettings CommonSettings;
static BaseDriver _stub_driver;
BaseDriver* driver = &_stub_driver;
std::string InputDisplayString;

static BOOL LidClosed = FALSE;
static u8 countLid = 0;
static NDSError _lastNDSError;

GameInfo gameInfo;
NDSSystem nds;
CFIRMWARE *extFirmwareObj = NULL;

std::vector<u32> memReadBreakPoints;
std::vector<u32> memWriteBreakPoints;

using std::min;
using std::max;

bool singleStep;
bool nds_debug_continuing[2];
int lagframecounter;
int LagFrameFlag;
int lastLag;
int TotalLagFrames;
u8 MicSampleSelection = 0;
std::vector< std::vector<u8> > micSamples;

TSCalInfo TSCal;

namespace DLDI
{
	bool tryPatch(void* data, size_t size, unsigned int device);
}

void Desmume_InitOnce()
{
	static bool initOnce = false;
	if(initOnce) return;
	initOnce = true;

#ifdef HAVE_LIBAGG
	extern void Agg_init(); //no need to include just for this
	Agg_init();
#endif
}

int NDS_GetCPUCoreCount()
{
   int amount = cpu_features_get_core_amount();
   return amount;
}

void NDS_SetupDefaultFirmware()
{
	NDS_GetDefaultFirmwareConfig(CommonSettings.fwConfig);
}

void NDS_RunAdvansceneAutoImport()
{
	if (CommonSettings.run_advanscene_import != "")
	{
		std::string fname = CommonSettings.run_advanscene_import;
		std::string fname_out = fname + ".ddb";
		EMUFILE_FILE outf(fname_out,"wb");
		u32 ret = advsc.convertDB(fname.c_str(),outf);
		if (ret == 0)
			exit(0);
		else exit(1);
	}
}

int NDS_Init()
{
	nds.idleFrameCounter = 0;
	memset(nds.runCycleCollector,0,sizeof(nds.runCycleCollector));
	MMU_Init();

	//got to print this somewhere..
	printf("%s\n", EMU_DESMUME_NAME_AND_VERSION());
	
	{
		char	buf[MAX_PATH];
		memset(buf, 0, MAX_PATH);
		strcpy(buf, path.pathToModule);
		strcat(buf, "desmume.ddb");							// DeSmuME database	:)
		advsc.setDatabase(buf);

		//why is this done here? shitty engineering. not intended.
		NDS_RunAdvansceneAutoImport();
	}
	
	armcpu_new(&NDS_ARM9,0);
	armcpu_SetBaseMemoryInterface(&NDS_ARM9, &arm9_base_memory_iface);
	armcpu_SetBaseMemoryInterfaceData(&NDS_ARM9, NULL);
	armcpu_ResetMemoryInterfaceToBase(&NDS_ARM9);
	
	armcpu_new(&NDS_ARM7,1);
	armcpu_SetBaseMemoryInterface(&NDS_ARM7, &arm7_base_memory_iface);
	armcpu_SetBaseMemoryInterfaceData(&NDS_ARM7, NULL);
	armcpu_ResetMemoryInterfaceToBase(&NDS_ARM7);
	
	delete GPU;
	GPU = new GPUSubsystem;
	
	if (SPU_Init(SNDCORE_DUMMY, 740) != 0)
		return -1;
	
	delete wifiHandler;
	wifiHandler = new WifiHandler;

	cheats = new CHEATS();
	cheatSearch = new CHEATSEARCH();

	return 0;
}

void NDS_DeInit(void)
{
	gameInfo.closeROM();
	SPU_DeInit();
	
	delete GPU;
	GPU = NULL;
	
	MMU_DeInit();
	
	delete wifiHandler;
	wifiHandler = NULL;
	
	delete cheats;
	cheats = NULL;
	
	delete cheatSearch;
	cheatSearch = NULL;

#ifdef HAVE_JIT
	arm_jit_close();
#endif

#ifdef LOG_ARM7
	if (fp_dis7 != NULL) 
	{
		fclose(fp_dis7);
		fp_dis7 = NULL;
	}
#endif

#ifdef LOG_ARM9
	if (fp_dis9 != NULL) 
	{
		fclose(fp_dis9);
		fp_dis9 = NULL;
	}
#endif
}

NDS_header* NDS_getROMHeader(void)
{
	NDS_header *newHeader = new NDS_header;
	memcpy(newHeader, &gameInfo.header, sizeof(NDS_header));
	
	return newHeader;
} 




void debug()
{
	//if(NDS_ARM9.R[15]==0x020520DC) emu_halt();
	//DSLinux
	//if(NDS_ARM9.CPSR.bits.mode == 0) emu_halt();
	//if((NDS_ARM9.R[15]&0xFFFFF000)==0) emu_halt();
	//if((NDS_ARM9.R[15]==0x0201B4F4)/*&&(NDS_ARM9.R[1]==0x0)*/) emu_halt();
	//AOE
	//if((NDS_ARM9.R[15]==0x01FFE194)&&(NDS_ARM9.R[0]==0)) emu_halt();
	//if((NDS_ARM9.R[15]==0x01FFE134)&&(NDS_ARM9.R[0]==0)) emu_halt();

	//BBMAN
	//if(NDS_ARM9.R[15]==0x02098B4C) emu_halt();
	//if(NDS_ARM9.R[15]==0x02004924) emu_halt();
	//if(NDS_ARM9.R[15]==0x02004890) emu_halt();

	//if(NDS_ARM9.R[15]==0x0202B800) emu_halt();
	//if(NDS_ARM9.R[15]==0x0202B3DC) emu_halt();
	//if((NDS_ARM9.R[1]==0x9AC29AC1)&&(!fait)) {emu_halt();fait = TRUE;}
	//if(NDS_ARM9.R[1]==0x0400004A) {emu_halt();fait = TRUE;}
	/*if(NDS_ARM9.R[4]==0x2E33373C) emu_halt();
	if(NDS_ARM9.R[15]==0x02036668) //emu_halt();
	{
	nds.logcount++;
	sprintf(logbuf, "%d %08X", nds.logcount, NDS_ARM9.R[13]);
	log::ajouter(logbuf);
	if(nds.logcount==89) execute=FALSE;
	}*/
	//if(NDS_ARM9.instruction==0) emu_halt();
	//if((NDS_ARM9.R[15]>>28)) emu_halt();
}

#if 0 /* not used */
//http://www.aggregate.org/MAGIC/#Population%20Count%20(Ones%20Count)
static u32 ones32(u32 x)
{
	/* 32-bit recursive reduction using SWAR...
	but first step is mapping 2-bit values
	into sum of 2 1-bit values in sneaky way
	*/
	x -= ((x >> 1) & 0x55555555);
	x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
	x = (((x >> 4) + x) & 0x0f0f0f0f);
	x += (x >> 8);
	x += (x >> 16);
	return(x & 0x0000003f);
}
#endif

RomBanner::RomBanner(bool defaultInit)
{
	if(!defaultInit) return;
	version = 1; //Version  (0001h)
	crc16 = 0; //CRC16 across entries 020h..83Fh
	memset(reserved,0,sizeof(reserved));
	memset(bitmap,0,sizeof(bitmap));
	memset(palette,0,sizeof(palette));
	memset(titles,0,sizeof(titles));
	memset(end0xFF,0,sizeof(end0xFF));
}

bool GameInfo::hasRomBanner()
{
	if(header.IconOff + sizeof(RomBanner) > romsize)
		return false;
	else return true;
}

bool GameInfo::IsCode(const char* code) const
{
	return memcmp(code,header.gameCode,strlen(code))==0;
}

const RomBanner& GameInfo::getRomBanner()
{
	return banner;
}

bool GameInfo::ValidateHeader()
{
	bool isRomValid = false;
	
	// Validate the ROM type.
	int detectedRomType = DetectRomType(*(Header *)&header, (char *)secureArea);
	if (detectedRomType == ROMTYPE_INVALID)
	{
		printf("ROM Validation: Invalid ROM type detected.\n");
		return isRomValid;
	}
	
	// Ensure that the game title and game code are both clean ASCII, but also
	// make an exception for homebrew ROMs, which may not always have clean
	// headers to begin with.
	if (detectedRomType != ROMTYPE_HOMEBREW)
	{
		for (size_t i = 0; i < 12; i++)
		{
			char c = (char)header.gameTile[i];
			if (c < 0 || (c > 0 && c < 32) || c == 127)
			{
				printf("ROM Validation: Invalid character detected in ROM Title.\n");
				printf("                charIndex = %d, charValue = %d\n", (int)i, c);
				return isRomValid;
			}
		}
		
		for (size_t i = 0; i < 4; i++)
		{
			char c = (char)header.gameCode[i];
			if (c < 0 || (c > 0 && c < 32) || c == 127)
			{
				printf("ROM Validation: Invalid character detected in ROM Game Code.\n");
				printf("                charIndex = %d, charValue = %d\n", (int)i, c);
				return isRomValid;
			}
		}
	}
	
	isRomValid = true;
	return isRomValid;
}

void GameInfo::populate()
{

	
	//set or build as appropriate ROMserial
	if(isHomebrew())
	{
		//we can't really make a serial for a homebrew game that hasnt set a game code
		strcpy(ROMserial, "Homebrew");
	}
	else
	{
		if (isDSiEnhanced())
			strcpy(ROMserial,"TWL-    -");
		else
			strcpy(ROMserial,"NTR-    -");
		memcpy(ROMserial+4, header.gameCode, 4);

		const char* rgn = Database::RegionXXXForCode(header.gameCode[3],true);
		strcat(ROMserial, rgn);
	}

	//rom name is probably set even in homebrew, so do it regardless
	memcpy(ROMname, header.gameTile, 12);
	ROMname[12] = 0;

		/*if(header.IconOff < romsize)
		{
			u8 num = (T1ReadByte((u8*)romdata, header.IconOff) == 1)?6:7;
			for (int i = 0; i < num; i++)
			{
				wcstombs(ROMfullName[i], (wchar_t *)(romdata+header.IconOff+0x240+(i*0x100)), 0x100);
				trim(ROMfullName[i]);
			}
		}*/
}

bool GameInfo::loadROM(std::string fname, u32 type)
{
	//printf("ROM %s\n", CommonSettings.loadToMemory?"loaded to RAM":"stream from disk");

	closeROM();

	char *noext = strdup(fname.c_str());
	reader = ROMReaderInit(&noext); free(noext);
	fROM = reader->Init(fname.c_str());
	if (!fROM) return false;

	headerOffset = (type == ROM_DSGBA)?DSGBA_LOADER_SIZE:0;
	romsize = reader->Size(fROM) - headerOffset;
	reader->Seek(fROM, headerOffset, SEEK_SET);

	bool res = (reader->Read(fROM, &header, sizeof(header)) == sizeof(header));

	if (res)
	{
#ifdef MSB_FIRST
		//endian swap necessary fields. It would be better if we made accessors for these. I wonder if you could make a macro for a field accessor that would take the bitsize and do the swap on the fly
		struct FieldSwap {
			const size_t offset;
			const size_t bytes;
		};
		
		static const FieldSwap fieldSwaps[] = {
			{ offsetof(NDS_header,makerCode), 2},
			
			{ offsetof(NDS_header,ARM9src), 4},
			{ offsetof(NDS_header,ARM9exe), 4},
			{ offsetof(NDS_header,ARM9cpy), 4},
			{ offsetof(NDS_header,ARM9binSize), 4},
			{ offsetof(NDS_header,ARM7src), 4},
			{ offsetof(NDS_header,ARM7exe), 4},
			{ offsetof(NDS_header,ARM7cpy), 4},
			{ offsetof(NDS_header,ARM7binSize), 4},
			{ offsetof(NDS_header,FNameTblOff), 4},
			{ offsetof(NDS_header,FNameTblSize), 4},
			{ offsetof(NDS_header,FATOff), 4},
			{ offsetof(NDS_header,FATSize), 4},
			{ offsetof(NDS_header,ARM9OverlayOff), 4},
			{ offsetof(NDS_header,ARM9OverlaySize), 4},
			{ offsetof(NDS_header,ARM7OverlayOff), 4},
			{ offsetof(NDS_header,ARM7OverlaySize), 4},
			{ offsetof(NDS_header,normalCmd), 4},
			{ offsetof(NDS_header,Key1Cmd), 4},
			{ offsetof(NDS_header,IconOff), 4},
			
			{ offsetof(NDS_header,CRC16), 2},
			{ offsetof(NDS_header,ROMtimeout), 2},
			
			{ offsetof(NDS_header,ARM9autoload), 4},
			{ offsetof(NDS_header,ARM7autoload), 4},
			{ offsetof(NDS_header,endROMoffset), 4},
			{ offsetof(NDS_header,HeaderSize), 4},
			
			{ offsetof(NDS_header, ARM9module), 4},
			{ offsetof(NDS_header, ARM7module), 4},
			
			{ offsetof(NDS_header,logoCRC16), 2},
			{ offsetof(NDS_header,headerCRC16), 2},
		};
		
		for(size_t i = 0; i < ARRAY_SIZE(fieldSwaps); i++)
		{
			const u8 *fieldAddr = (u8 *)&header + fieldSwaps[i].offset;
			
			switch(fieldSwaps[i].bytes)
			{
				case 2:
					*(u16 *)fieldAddr = LE_TO_LOCAL_16(*(u16 *)fieldAddr);
					break;
					
				case 4:
					*(u32 *)fieldAddr = LE_TO_LOCAL_32(*(u32 *)fieldAddr);
					break;
			}
		}
#endif
		cardSize = (128 * 1024) << header.cardSize;

		if (cardSize < romsize)
		{
			msgbox->warn("The ROM header is invalid.\nThe device size has been increased to allow for the provided file size.\n");
			
			for (u32 i = header.cardSize; i < 0xF; i++)
			{
				if (((128 * 1024) << i) >= romsize) 
				{
					header.cardSize = i;
					cardSize = (128 * 1024) << i;
					break;
				}
			}
		}
		
		mask = (cardSize - 1);
		mask |= (mask >>1);
		mask |= (mask >>2);
		mask |= (mask >>4);
		mask |= (mask >>8);
		mask |= (mask >>16);

		if (type == ROM_NDS)
		{
			reader->Seek(fROM, 0x4000 + headerOffset, SEEK_SET);
			reader->Read(fROM, &secureArea[0], 0x4000);
		}

		//for now, we have to do this, because the DLDI patching requires it
		bool loadToMemory = CommonSettings.loadToMemory;
		if(isHomebrew())
			loadToMemory = true;

		//convert to an in-memory reader around a pre-read buffer if that's what's requested
		if (loadToMemory)
		{
			reader->Seek(fROM, headerOffset, SEEK_SET);
			
			romdataForReader = new u8[romsize];
			if (reader->Read(fROM, romdataForReader, romsize) != romsize)
			{
				delete [] romdataForReader; romdataForReader = NULL;
				romsize = 0;

				return false;
			}

			reader->DeInit(fROM); 
			fROM = NULL;
			reader = MemROMReaderRead_TrueInit(romdataForReader, romsize);
			fROM = reader->Init(NULL);
		}

		if(hasRomBanner())
		{
			reader->Seek(fROM, header.IconOff, SEEK_SET);
			reader->Read(fROM, &banner, sizeof(RomBanner));
				
			banner.version = LE_TO_LOCAL_16(banner.version);
			banner.crc16 = LE_TO_LOCAL_16(banner.crc16);
				
			for(size_t i = 0; i < ARRAY_SIZE(banner.palette); i++)
			{
				banner.palette[i] = LE_TO_LOCAL_16(banner.palette[i]);
			}
		}

		_isDSiEnhanced = ((readROM(0x180) == 0x8D898581U) && (readROM(0x184) == 0x8C888480U));
		if (hasRomBanner())
		{
			reader->Seek(fROM, header.IconOff + headerOffset, SEEK_SET);
			reader->Read(fROM, &banner, sizeof(RomBanner));
			
			banner.version = LE_TO_LOCAL_16(banner.version);
			banner.crc16 = LE_TO_LOCAL_16(banner.crc16);
			
			for(size_t i = 0; i < ARRAY_SIZE(banner.palette); i++)
			{
				banner.palette[i] = LE_TO_LOCAL_16(banner.palette[i]);
			}
		}
		reader->Seek(fROM, headerOffset, SEEK_SET);
		return true;
	}

	romsize = 0;
	reader->DeInit(fROM); fROM = NULL;
	return false;
}

void GameInfo::closeROM()
{
	if (wifiHandler != NULL)
		wifiHandler->CommStop();
	
	if (GPU != NULL)
		GPU->ForceFrameStop();
	
	if (reader != NULL)
		reader->DeInit(fROM);

	if (romdataForReader != NULL)
		delete [] romdataForReader;

	fROM = NULL;
	reader = NULL;
	romdataForReader = NULL;
	romsize = 0;
}

u32 GameInfo::readROM(u32 pos)
{
	u32 num;
	u32 data;

	//reader must try to be efficient and not do unneeded seeks
	reader->Seek(fROM, pos, SEEK_SET);
	num = reader->Read(fROM, &data, 4);

	//in case we didn't read enough data, pad the remainder with 0xFF
	u32 pad = 0;
	while(num<4)
	{
		pad >>= 8;
		pad |= 0xFF000000;
		num++;
	}

	return (LE_TO_LOCAL_32(data) & ~pad) | pad;
}

bool GameInfo::isDSiEnhanced()
{
	return _isDSiEnhanced;
}

bool GameInfo::isHomebrew()
{
	return ((header.ARM9src < 0x4000) && (T1ReadLong(header.logo, 0) != 0x51AEFF24) && (T1ReadLong(header.logo, 4) != 0x699AA221));
}

static int rom_init_path(const char *filename, const char *physicalName, const char *logicalFilename)
{
	u32	type = ROM_NDS;

	path.init(logicalFilename? logicalFilename : filename);

	if (!strcasecmp(path.extension().c_str(),"zip")
		|| !strcasecmp(path.extension().c_str(),"gz")) {
			type = ROM_NDS;
			gameInfo.loadROM(path.path, type);
	} else
	if ( path.isdsgba(path.path)) {
		type = ROM_DSGBA;
		gameInfo.loadROM(path.path, type);
	}
	else if ( !strcasecmp(path.extension().c_str(), "nds")) {
		type = ROM_NDS;
		gameInfo.loadROM(physicalName ? physicalName : path.path, type); //n.b. this does nothing if the file can't be found (i.e. if it was an extracted tempfile)...
		//...but since the data was extracted to gameInfo then it is ok
	}
	//ds.gba in archives, it's already been loaded into memory at this point
	else if (logicalFilename && path.isdsgba(std::string(logicalFilename))) {
		type = ROM_DSGBA;
	} else {
		//well, try to load it as an nds rom anyway
		type = ROM_NDS;
		gameInfo.loadROM(physicalName ? physicalName : path.path, type);
	}

	//check that size is at least the size of the header
	if (gameInfo.romsize < 352) {
		return -1;
	}

	gameInfo.romType = type;

	return 1;
}

struct LastRom {
	std::string filename, physicalName, logicalFilename;
} lastRom;

int NDS_LoadROM(const char *filename, const char *physicalName, const char *logicalFilename)
{
	lastRom.filename = filename;
	lastRom.physicalName = physicalName ? physicalName : "";
	lastRom.logicalFilename = logicalFilename ? logicalFilename : "";

	int	ret;
	char	buf[MAX_PATH];

	if (filename == NULL)
		return -1;

	ret = rom_init_path(filename, physicalName, logicalFilename);
	if (ret < 1)
		return ret;

	if (cheatSearch)
		cheatSearch->close();
	FCEUI_StopMovie();
	
	if (!gameInfo.ValidateHeader())
	{
		ret = -1;
		return ret;
	}
	
	gameInfo.populate();
	
	//run crc over the whole buffer (chunk at a time, to avoid coding a streaming crc
	gameInfo.reader->Seek(gameInfo.fROM, 0, SEEK_SET);
	gameInfo.crc = 0;
	
	u8 fROMBuffer[4096];
	bool first = true;
	
	for(;;) {
		int read = gameInfo.reader->Read(gameInfo.fROM,fROMBuffer,4096);
		if(read == 0) break;
		if(first && read >= 512)
			gameInfo.crcForCheatsDb = ~crc32(0, fROMBuffer, 512);
		first = false;
		gameInfo.crc = crc32(gameInfo.crc, fROMBuffer, read);
	}

	gameInfo.chipID  = 0xC2;														// The Manufacturer ID is defined by JEDEC (C2h = Macronix)
	if (!gameInfo.isHomebrew())
	{
		gameInfo.chipID |= ((((128 << gameInfo.header.cardSize) / 1024) - 1) << 8);		// Chip size in megabytes minus 1
																						// (07h = 8MB, 0Fh = 16MB, 1Fh = 32MB, 3Fh = 64MB, 7Fh = 128MB)

		// flags
		// 0: Unknown
		// 1: Unknown
		// 2: Unknown
		// 3: Unknown
		// 4: Unknown
		// 5: DSi? (if set to 1 then DSi Enhanced games send command D6h to Slot1)
		// 6: Unknown
		// 7: ROM speed (Secure Area Block transfer mode (trasfer 8x200h or 1000h bytes)
		// TODO:
		//if (gameInfo.isDSiEnhanced())
		//		gameInfo.chipID |= (0x40 << 24);
		gameInfo.chipID |= (0x00 << 24);
	}

	INFO("\nROM game code: %c%c%c%c\n", gameInfo.header.gameCode[0], gameInfo.header.gameCode[1], gameInfo.header.gameCode[2], gameInfo.header.gameCode[3]);
	INFO("ROM crc: %08X\n", gameInfo.crc);
	if (!gameInfo.isHomebrew())
	{
		INFO("ROM serial: %s\n", gameInfo.ROMserial);
		INFO("ROM chipID: %08X\n", gameInfo.chipID);
		INFO("ROM internal name: %s\n", gameInfo.ROMname);
		if (gameInfo.isDSiEnhanced()) INFO("ROM DSi Enhanced\n");
	}

	const char *makerName = Database::MakerNameForMakerCode(gameInfo.header.makerCode,true);
	INFO("ROM developer: %s\n", ((gameInfo.header.makerCode == 0) && gameInfo.isHomebrew())?"Homebrew":makerName);

	buf[0] = gameInfo.header.gameCode[0];
	buf[1] = gameInfo.header.gameCode[1];
	buf[2] = gameInfo.header.gameCode[2];
	buf[3] = gameInfo.header.gameCode[3];
	buf[4] = 0;
	if (advsc.checkDB(buf, gameInfo.crc))
	{
		u8 sv = advsc.getSaveType();
		printf("Found in game database by %s:\n", advsc.getIdMethod());
		printf("\t* ROM serial:\t\t%s\n", advsc.getSerial());
		printf("\t* ROM save type:\t");
		if (sv == 0xFF)
			printf("Unknown");
		else
			if (sv == 0xFE)
				printf("None");
			else
			{
				printf("%s", save_types[sv + 1].descr);
				if (CommonSettings.autodetectBackupMethod == 1)
					backup_setManualBackupType(sv + 1);
			}
		printf("\n\t* ROM crc:\t\t%08X\n", advsc.getCRC32());
	}
	printf("\n");

	//for homebrew, try auto-patching DLDI. should be benign if there is no DLDI or if it fails
	if(gameInfo.isHomebrew())
	{
		//note: gameInfo.romdataForReader is safe here because we made sure to load the rom into memory for isHomebrew
		if (slot1_GetCurrentType() == NDS_SLOT1_R4)
			DLDI::tryPatch((void*)gameInfo.romdataForReader, gameInfo.romsize, 1);
		else if (slot2_GetCurrentType() == NDS_SLOT2_CFLASH)
			DLDI::tryPatch((void*)gameInfo.romdataForReader, gameInfo.romsize, 0);
	}

	if (cheats != NULL)
	{
		memset(buf, 0, MAX_PATH);
		path.getpathnoext(path.CHEATS, buf);
		strcat(buf, ".dct");						// DeSmuME cheat		:)
		cheats->init(buf);
	}

	//UnloadMovieEmulationSettings(); called in NDS_Reset()
	NDS_Reset();

	return ret;
}

void NDS_FreeROM(void)
{
	FCEUI_StopMovie();
	gameInfo.closeROM();
	UnloadMovieEmulationSettings();
}

void NDS_Sleep() { nds.sleeping = TRUE; }

void NDS_TriggerCardEjectIRQ()
{
	NDS_makeIrq(ARMCPU_ARM7, IRQ_BIT_GC_IREQ_MC);
	NDS_makeIrq(ARMCPU_ARM9, IRQ_BIT_GC_IREQ_MC); //zero added on 11-aug-2013 with no proof of accuracy
}


class FrameSkipper
{
public:
	void RequestSkip()
	{
		nextSkip = true;
	}
	void OmitSkip(bool force, bool forceEvenIfCapturing=false)
	{
		nextSkip = false;
		if((force && consecutiveNonCaptures > 30) || forceEvenIfCapturing)
		{
			SkipCur2DFrame = false;
			SkipCur3DFrame = false;
			SkipNext2DFrame = false;
			if(forceEvenIfCapturing)
				consecutiveNonCaptures = 0;
		}
	}
	void Advance()
	{
		const GPUEngineA *mainEngine = GPU->GetEngineMain();
		const IOREG_DISPCAPCNT &DISPCAPCNT = mainEngine->GetIORegisterMap().DISPCAPCNT;
		const bool capturing = (DISPCAPCNT.CaptureEnable != 0);

		if(capturing && consecutiveNonCaptures > 30)
		{
			// the worst-looking graphics corruption problems from frameskip
			// are the result of skipping the capture on first frame it turns on.
			// so we do this to handle the capture immediately,
			// despite the risk of 1 frame of 2d/3d mismatch or wrong screen display.
			SkipNext2DFrame = false;
			nextSkip = false;
		}
		else if((lastDisplayTarget != mainEngine->GetTargetDisplayByID()) && lastSkip && !skipped)
		{
			// if we're switching from not skipping to skipping
			// and the screens are also switching around this frame,
			// go for 1 extra frame without skipping.
			// this avoids the scenario where we only draw one of the two screens
			// when a game is switching screens every frame.
			nextSkip = false;
		}

		if(capturing)
			consecutiveNonCaptures = 0;
		else if(!(consecutiveNonCaptures > 9000)) // arbitrary cap to avoid eventual wrap
			consecutiveNonCaptures++;
		
		lastDisplayTarget = mainEngine->GetTargetDisplayByID();
		lastSkip = skipped;
		skipped = nextSkip;
		nextSkip = false;

		SkipCur2DFrame = SkipNext2DFrame;
		SkipCur3DFrame = skipped;
		SkipNext2DFrame = skipped;
	}
	FORCEINLINE bool ShouldSkip2D()
	{
		return SkipCur2DFrame;
	}
	FORCEINLINE bool ShouldSkip3D()
	{
		return SkipCur3DFrame;
	}
	FrameSkipper()
	{
		nextSkip = false;
		skipped = false;
		lastSkip = false;
		lastDisplayTarget = NDSDisplayID_Main;
		SkipCur2DFrame = false;
		SkipCur3DFrame = false;
		SkipNext2DFrame = false;
		consecutiveNonCaptures = 0;
	}
private:
	bool nextSkip;
	bool skipped;
	bool lastSkip;
	NDSDisplayID lastDisplayTarget;
	int consecutiveNonCaptures;
	bool SkipCur2DFrame;
	bool SkipCur3DFrame;
	bool SkipNext2DFrame;
};
static FrameSkipper frameSkipper;


void NDS_SkipNextFrame() {
	if (!driver->AVI_IsRecording()) {
		frameSkipper.RequestSkip();
	}
}
void NDS_OmitFrameSkip(int force) {
	frameSkipper.OmitSkip(force > 0, force > 1);
}

#define INDEX(i) ((((i)>>16)&0xFF0)|(((i)>>4)&0xF))


enum ESI_DISPCNT
{
	ESI_DISPCNT_HStart, ESI_DISPCNT_HStartIRQ, ESI_DISPCNT_HDraw, ESI_DISPCNT_HBlank
};

u64 nds_timer;
u64 nds_arm9_timer, nds_arm7_timer;

static const u64 kNever = 0xFFFFFFFFFFFFFFFFULL;

struct TSequenceItem
{
	u64 timestamp;
	u32 param;
	bool enabled;

	virtual void save(EMUFILE &os)
	{
		os.write_64LE(timestamp);
		os.write_32LE(param);
		os.write_bool32(enabled);
	}

	virtual bool load(EMUFILE &is)
	{
		if (is.read_64LE(timestamp) != 1) return false;
		if (is.read_32LE(param) != 1) return false;
		if (is.read_bool32(enabled) != 1) return false;
		return true;
	}

	FORCEINLINE bool isTriggered()
	{
		return enabled && nds_timer >= timestamp;
	}

	FORCEINLINE u64 next()
	{
		return timestamp;
	}
};

struct TSequenceItem_GXFIFO : public TSequenceItem
{
	FORCEINLINE bool isTriggered()
	{
		return enabled && nds_timer >= MMU.gfx3dCycles;
	}

	FORCEINLINE void exec()
	{
		IF_DEVELOPER(DEBUG_statistics.sequencerExecutionCounters[4]++);
		while(isTriggered()) {
			enabled = false;
			gfx3d_execute3D();
		}
	}

	FORCEINLINE u64 next()
	{
		if(enabled) return MMU.gfx3dCycles;
		else return kNever;
	}
};

template<int procnum, int num> struct TSequenceItem_Timer : public TSequenceItem
{
	FORCEINLINE bool isTriggered()
	{
		return enabled && nds_timer >= nds.timerCycle[procnum][num];
	}

	FORCEINLINE void schedule()
	{
		enabled = MMU.timerON[procnum][num] && MMU.timerMODE[procnum][num] != 0xFFFF;
	}

	FORCEINLINE u64 next()
	{
		return nds.timerCycle[procnum][num];
	}

	FORCEINLINE void exec()
	{
		IF_DEVELOPER(DEBUG_statistics.sequencerExecutionCounters[13+procnum*4+num]++);
		u8* regs = procnum==0?MMU.ARM9_REG:MMU.ARM7_REG;
		bool first = true;
		//we'll need to check chained timers..
		for(int i=num;i<4;i++)
		{
			bool over = false;
			//maybe too many checks if this is here, but we need it here for now
			if(!MMU.timerON[procnum][i]) return;

			if(MMU.timerMODE[procnum][i] == 0xFFFF)
			{
				++(MMU.timer[procnum][i]);
				over = !MMU.timer[procnum][i];
			}
			else
			{
				if(!first) break; //this timer isn't chained. break the chain
				first = false;

				over = true;
				int remain = 65536 - MMU.timerReload[procnum][i];
				int ctr=0;
				while(nds.timerCycle[procnum][i] <= nds_timer) {
					nds.timerCycle[procnum][i] += (remain << MMU.timerMODE[procnum][i]);
					ctr++;
				}
#ifndef NDEBUG
				if(ctr>1) {
					printf("yikes!!!!! please report!\n");
				}
#endif
			}

			if(over)
			{
				MMU.timer[procnum][i] = MMU.timerReload[procnum][i];
				if(T1ReadWord(regs, 0x102 + i*4) & 0x40) 
				{
					NDS_makeIrq(procnum, IRQ_BIT_TIMER_0 + i);
				}
			}
			else
				break; //no more chained timers to trigger. we're done here
		}
	}
};

template<int procnum, int chan> struct TSequenceItem_DMA : public TSequenceItem
{
	DmaController* controller;

	FORCEINLINE bool isTriggered()
	{
		return (controller->dmaCheck && nds_timer>= controller->nextEvent);
	}

	FORCEINLINE bool isEnabled() { 
		return controller->dmaCheck?TRUE:FALSE;
	}

	FORCEINLINE u64 next()
	{
		return controller->nextEvent;
	}

	FORCEINLINE void exec()
	{
		IF_DEVELOPER(DEBUG_statistics.sequencerExecutionCounters[5+procnum*4+chan]++);

		//if (nds.freezeBus) return;

		//printf("exec from TSequenceItem_DMA: %d %d\n",procnum,chan);
		controller->exec();
//		//give gxfifo dmas a chance to re-trigger
//		if(MMU.DMAStartTime[procnum][chan] == EDMAMode_GXFifo) {
//			MMU.DMAing[procnum][chan] = FALSE;
//			if (gxFIFO.size <= 127) 
//			{
//				execHardware_doDma(procnum,chan,EDMAMode_GXFifo);
//				if (MMU.DMACompleted[procnum][chan])
//					goto docomplete;
//				else return;
//			}
//		}
//
//docomplete:
//		if (MMU.DMACompleted[procnum][chan])	
//		{
//			u8* regs = procnum==0?MMU.ARM9_REG:MMU.ARM7_REG;
//
//			//disable the channel
//			if(MMU.DMAStartTime[procnum][chan] != EDMAMode_GXFifo) {
//				T1WriteLong(regs, 0xB8 + (0xC*chan), T1ReadLong(regs, 0xB8 + (0xC*chan)) & 0x7FFFFFFF);
//				MMU.DMACrt[procnum][chan] &= 0x7FFFFFFF; //blehhh i hate this shit being mirrored in memory
//			}
//
//			if((MMU.DMACrt[procnum][chan])&(1<<30)) {
//				if(procnum==0) NDS_makeARM9Int(8+chan);
//				else NDS_makeARM7Int(8+chan);
//			}
//
//			MMU.DMAing[procnum][chan] = FALSE;
//		}

	}
};

struct TSequenceItem_ReadSlot1 : public TSequenceItem
{
	FORCEINLINE bool isTriggered()
	{
		return enabled && nds_timer >= timestamp;
	}

	bool isEnabled() { return this->enabled; }

	FORCEINLINE u64 next()
	{
		return timestamp;
	}

	void exec()
	{
		u32 procnum = param;
		enabled = false;
		u32 val = T1ReadLong(MMU.MMU_MEM[procnum][0x40], 0x1A4);
		val |= 0x00800000;
		T1WriteLong(MMU.MMU_MEM[procnum][0x40], 0x1A4, val);
		triggerDma(EDMAMode_Card);
	}

};

struct TSequenceItem_divider : public TSequenceItem
{
	FORCEINLINE bool isTriggered()
	{
		return MMU.divRunning && nds_timer >= MMU.divCycles;
	}

	bool isEnabled() { return MMU.divRunning!=0; }

	FORCEINLINE u64 next()
	{
		return MMU.divCycles;
	}

	void exec()
	{
		IF_DEVELOPER(DEBUG_statistics.sequencerExecutionCounters[2]++);
		MMU_new.div.busy = 0;
#ifdef HOST_64 
		T1WriteQuad(MMU.ARM9_REG, 0x2A0, MMU.divResult);
		T1WriteQuad(MMU.ARM9_REG, 0x2A8, MMU.divMod);
#else
		T1WriteLong(MMU.ARM9_REG, 0x2A0, (u32)MMU.divResult);
		T1WriteLong(MMU.ARM9_REG, 0x2A4, (u32)(MMU.divResult >> 32));
		T1WriteLong(MMU.ARM9_REG, 0x2A8, (u32)MMU.divMod);
		T1WriteLong(MMU.ARM9_REG, 0x2AC, (u32)(MMU.divMod >> 32));
#endif
		MMU.divRunning = FALSE;
	}

};

struct TSequenceItem_sqrtunit : public TSequenceItem
{
	FORCEINLINE bool isTriggered()
	{
		return MMU.sqrtRunning && nds_timer >= MMU.sqrtCycles;
	}

	bool isEnabled() { return MMU.sqrtRunning!=0; }

	FORCEINLINE u64 next()
	{
		return MMU.sqrtCycles;
	}

	FORCEINLINE void exec()
	{
		IF_DEVELOPER(DEBUG_statistics.sequencerExecutionCounters[3]++);
		MMU_new.sqrt.busy = 0;
		T1WriteLong(MMU.ARM9_REG, 0x2B4, MMU.sqrtResult);
		MMU.sqrtRunning = FALSE;
	}

};

struct Sequencer
{
	bool nds_vblankEnded;
	bool reschedule;
	TSequenceItem dispcnt;
	TSequenceItem wifi;
	TSequenceItem_divider divider;
	TSequenceItem_sqrtunit sqrtunit;
	TSequenceItem_GXFIFO gxfifo;
	TSequenceItem_ReadSlot1 readslot1;
	TSequenceItem_DMA<0,0> dma_0_0; TSequenceItem_DMA<0,1> dma_0_1; 
	TSequenceItem_DMA<0,2> dma_0_2; TSequenceItem_DMA<0,3> dma_0_3; 
	TSequenceItem_DMA<1,0> dma_1_0; TSequenceItem_DMA<1,1> dma_1_1; 
	TSequenceItem_DMA<1,2> dma_1_2; TSequenceItem_DMA<1,3> dma_1_3; 
	TSequenceItem_Timer<0,0> timer_0_0; TSequenceItem_Timer<0,1> timer_0_1;
	TSequenceItem_Timer<0,2> timer_0_2; TSequenceItem_Timer<0,3> timer_0_3;
	TSequenceItem_Timer<1,0> timer_1_0; TSequenceItem_Timer<1,1> timer_1_1;
	TSequenceItem_Timer<1,2> timer_1_2; TSequenceItem_Timer<1,3> timer_1_3;

	void init();

	void execHardware();
	u64 findNext();

	void save(EMUFILE &os)
	{
		os.write_64LE(nds_timer);
		os.write_64LE(nds_arm9_timer);
		os.write_64LE(nds_arm7_timer);
		dispcnt.save(os);
		divider.save(os);
		sqrtunit.save(os);
		gxfifo.save(os);
		readslot1.save(os);
		wifi.save(os);
#define SAVE(I,X,Y) I##_##X##_##Y .save(os);
		SAVE(timer,0,0); SAVE(timer,0,1); SAVE(timer,0,2); SAVE(timer,0,3); 
		SAVE(timer,1,0); SAVE(timer,1,1); SAVE(timer,1,2); SAVE(timer,1,3); 
		SAVE(dma,0,0); SAVE(dma,0,1); SAVE(dma,0,2); SAVE(dma,0,3); 
		SAVE(dma,1,0); SAVE(dma,1,1); SAVE(dma,1,2); SAVE(dma,1,3); 
#undef SAVE
	}

	bool load(EMUFILE &is, int version)
	{
		if (is.read_64LE(nds_timer) != 1) return false;
		if (is.read_64LE(nds_arm9_timer) != 1) return false;
		if (is.read_64LE(nds_arm7_timer) != 1) return false;
		if (!dispcnt.load(is)) return false;
		if (!divider.load(is)) return false;
		if (!sqrtunit.load(is)) return false;
		if (!gxfifo.load(is)) return false;
		if (version >= 4) if (!readslot1.load(is)) return false;
		if (version >= 1) if(!wifi.load(is)) return false;
#define LOAD(I,X,Y) if(!I##_##X##_##Y .load(is)) return false;
		LOAD(timer,0,0); LOAD(timer,0,1); LOAD(timer,0,2); LOAD(timer,0,3); 
		LOAD(timer,1,0); LOAD(timer,1,1); LOAD(timer,1,2); LOAD(timer,1,3); 
		LOAD(dma,0,0); LOAD(dma,0,1); LOAD(dma,0,2); LOAD(dma,0,3); 
		LOAD(dma,1,0); LOAD(dma,1,1); LOAD(dma,1,2); LOAD(dma,1,3); 
#undef LOAD

		return true;
	}

} sequencer;

void NDS_RescheduleGXFIFO(u32 cost)
{
	if(!sequencer.gxfifo.enabled) {
		MMU.gfx3dCycles = nds_timer;
		sequencer.gxfifo.enabled = true;
	}
	MMU.gfx3dCycles += cost;
	NDS_Reschedule();
}

void NDS_RescheduleTimers()
{
#define check(X,Y) sequencer.timer_##X##_##Y .schedule();
	check(0,0); check(0,1); check(0,2); check(0,3);
	check(1,0); check(1,1); check(1,2); check(1,3);
#undef check

	NDS_Reschedule();
}

void NDS_RescheduleReadSlot1(int procnum, int size)
{
	u32 gcromctrl = T1ReadLong(MMU.MMU_MEM[procnum][0x40], 0x1A4);
	
	u32 clocks = (gcromctrl & (1<<27)) ? 8 : 5;
	u32 gap = gcromctrl & 0x1FFF;
	
	//time to send 8 command bytes, and then wait for the gap
	u32 delay = (8+gap)*clocks;

	//if data is to be returned, the first word is read before it's available and irqs and dmas fire
	if(size != 0) delay += 4;

	//timings are basically 33mhz but internal tracking is 66mhz
	delay *= 2;

	sequencer.readslot1.param = procnum;
	sequencer.readslot1.timestamp = nds_timer + delay;
	sequencer.readslot1.enabled = true;

	NDS_Reschedule();
}

void NDS_RescheduleDMA()
{
	//TBD
	NDS_Reschedule();

}

static void initSchedule()
{
	sequencer.init();

	//begin at the very end of the last scanline
	//so that at t=0 we can increment to scanline=0
	nds.VCount = 262; 

	sequencer.nds_vblankEnded = false;
}


// 2196372 ~= (ARM7_CLOCK << 16) / 1000000
// This value makes more sense to me, because:
// ARM7_CLOCK   = 33.51 mhz
//				= 33513982 cycles per second
// 				= 33.513982 cycles per microsecond
const u64 kWifiCycles = 67;//34*2;
//(this isn't very precise. I don't think it needs to be)

void Sequencer::init()
{
	NDS_RescheduleTimers();
	NDS_RescheduleDMA();

	reschedule = false;
	nds_timer = 0;
	nds_arm9_timer = 0;
	nds_arm7_timer = 0;

	dispcnt.enabled = true;
	dispcnt.param = ESI_DISPCNT_HStart;
	dispcnt.timestamp = 0;

	gxfifo.enabled = false;

	dma_0_0.controller = &MMU_new.dma[0][0];
	dma_0_1.controller = &MMU_new.dma[0][1];
	dma_0_2.controller = &MMU_new.dma[0][2];
	dma_0_3.controller = &MMU_new.dma[0][3];
	dma_1_0.controller = &MMU_new.dma[1][0];
	dma_1_1.controller = &MMU_new.dma[1][1];
	dma_1_2.controller = &MMU_new.dma[1][2];
	dma_1_3.controller = &MMU_new.dma[1][3];
	
	if (wifiHandler->GetCurrentEmulationLevel() != WifiEmulationLevel_Off)
	{
		wifi.enabled = true;
		wifi.timestamp = kWifiCycles;
	}
	else
	{
		wifi.enabled = false;
	}
}

static void execHardware_hblank()
{
	//this logic keeps moving around.
	//now, we try and give the game as much time as possible to finish doing its work for the scanline,
	//by drawing scanline N at the end of drawing time (but before subsequent interrupt or hdma-driven events happen)
	//don't try to do this at the end of the scanline, because some games (sonic classics) may use hblank IRQ to set
	//scroll regs for the next scanline
	if(nds.VCount<192)
	{
		if (nds.VCount == 0)
		{
			GPU->SetWillFrameSkip(frameSkipper.ShouldSkip2D());
		}
		
		GPU->RenderLine(nds.VCount);
		
		//trigger hblank dmas
		//but notice, we do that just after we finished drawing the line
		//(values copied by this hdma should not be used until the next scanline)
		triggerDma(EDMAMode_HBlank);
	}

	if(nds.VCount==262)
	{
		//we need to trigger one last hblank dma since 
		//a. we're sort of lagged behind by one scanline
		//b. i think that 193 hblanks actually fire (one for the hblank in scanline 262)
		//this is demonstrated by NSMB splot-parallaxing clouds
		//for some reason the game will setup two hdma scroll register buffers
		//to be run consecutively, and unless we do this, the second buffer will be offset by one scanline
		//causing a glitch in the 0th scanline
		//triggerDma(EDMAMode_HBlank);

		//BUT! this was removed in order to make glitches in megaman zero collection (mmz 4 1st level) work.
		//and, it seems that it is no longer necessary in nsmb. perhaps something else fixed it
	}


	//turn on hblank status bit
	T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) | 2);
	T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 2);

	//fire hblank interrupts if necessary
	if(T1ReadWord(MMU.ARM9_REG, 4) & 0x10) NDS_makeIrq(ARMCPU_ARM9,IRQ_BIT_LCD_HBLANK);
	if(T1ReadWord(MMU.ARM7_REG, 4) & 0x10) NDS_makeIrq(ARMCPU_ARM7,IRQ_BIT_LCD_HBLANK);

	//emulation housekeeping. for some reason we always do this at hblank,
	//even though it sounds more reasonable to do it at hstart
	SPU_Emulate_core();
	driver->AVI_SoundUpdate(SPU_core->outbuf,spu_core_samples);
	WAV_WavSoundUpdate(SPU_core->outbuf,spu_core_samples);
}

static void execHardware_hstart_vblankEnd()
{
	sequencer.nds_vblankEnded = true;
	sequencer.reschedule = true;

	//turn off vblank status bit
	T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) & ~1);
	T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & ~1);

	//some emulation housekeeping
	frameSkipper.Advance();
}

static void execHardware_hstart_vblankStart()
{
	//printf("--------VBLANK!!!--------\n");

	//fire vblank interrupts if necessary
	for(int i=0;i<2;i++)
	{
		if(MMU.reg_IF_pending[i] & (1<<IRQ_BIT_LCD_VBLANK))
		{
			MMU.reg_IF_pending[i] &= ~(1<<IRQ_BIT_LCD_VBLANK);
			NDS_makeIrq(i,IRQ_BIT_LCD_VBLANK);

			//for ARM7, cheats process when a vblank IRQ fires. necessary for AR compatibility and to stop cheats from breaking game boot-ups.
			//note that how we process raw cheats is up to us. so we'll do it the same way we used to, elsewhere
			if (i==1 && cheats)
				cheats->process(CHEAT_TYPE_AR);
		}
	}

	//trigger vblank dmas
	triggerDma(EDMAMode_VBlank);

	//tracking for arm9 load average
	nds.runCycleCollector[ARMCPU_ARM9][nds.idleFrameCounter] = 1120380-nds.idleCycles[0];
	nds.runCycleCollector[ARMCPU_ARM7][nds.idleFrameCounter] = 1120380-nds.idleCycles[1];
	nds.idleFrameCounter++;
	nds.idleFrameCounter &= 15;
	nds.idleCycles[0] = 0;
	nds.idleCycles[1] = 0;
}

static u16 execHardware_gen_vmatch_goal()
{
	u16 vmatch = T1ReadWord(MMU.ARM9_REG, 4);
	vmatch = ((vmatch>>8)|((vmatch<<1)&(1<<8)));
	return vmatch;
}

static void execHardware_hstart_vcount_irq()
{
	//trigger pending VMATCH irqs
	if(MMU.reg_IF_pending[ARMCPU_ARM9] & (1<<IRQ_BIT_LCD_VMATCH))
	{
		MMU.reg_IF_pending[ARMCPU_ARM9] &= ~(1<<IRQ_BIT_LCD_VMATCH);
		NDS_makeIrq(ARMCPU_ARM9,IRQ_BIT_LCD_VMATCH);
	}
	if(MMU.reg_IF_pending[ARMCPU_ARM7] & (1<<IRQ_BIT_LCD_VMATCH))
	{
		MMU.reg_IF_pending[ARMCPU_ARM7] &= ~(1<<IRQ_BIT_LCD_VMATCH);
		NDS_makeIrq(ARMCPU_ARM7,IRQ_BIT_LCD_VMATCH);
	}
}

static void execHardware_hstart_vcount()
{
	u16 vmatch = execHardware_gen_vmatch_goal();
	if(nds.VCount==vmatch)
	{
		//arm9 vmatch
		T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) | 4);
		if(T1ReadWord(MMU.ARM9_REG, 4) & 32) {
			MMU.reg_IF_pending[ARMCPU_ARM9] |= (1<<IRQ_BIT_LCD_VMATCH);
		}
	}
	else
		T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) & 0xFFFB);

	vmatch = T1ReadWord(MMU.ARM7_REG, 4);
	vmatch = ((vmatch>>8)|((vmatch<<1)&(1<<8)));
	if(nds.VCount==vmatch)
	{
		//arm7 vmatch
		T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 4);
		if(T1ReadWord(MMU.ARM7_REG, 4) & 32)
			MMU.reg_IF_pending[ARMCPU_ARM7] |= (1<<IRQ_BIT_LCD_VMATCH);
	}
	else
		T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFB);
}

static void execHardware_hdraw()
{
	//due to hacks in our selection of rendering time, we do not actually render here as intended.
	//consider changing this if there is some problem with raster fx timing but check the documentation near the gpu rendering calls
	//to make sure you check for regressions (nsmb, sonic classics, et al)
}

static void execHardware_hstart_irq()
{
	//this function very soon after the registers get updated to trigger IRQs
	//this is necessary to fix "egokoro kyoushitsu" which idles waiting for vcount=192, which never happens due to a long vblank irq
	//100% accurate emulation would require the read of VCOUNT to be in the pipeline already with the irq coming in behind it, thus 
	//allowing the vcount to register as 192 occasionally (maybe about 1 out of 28 frames)
	//the actual length of the delay is in execHardware() where the events are scheduled
	sequencer.reschedule = true;
	if(nds.VCount==192)
	{
		//when the vcount hits 192, vblank begins
		execHardware_hstart_vblankStart();
	}

	execHardware_hstart_vcount_irq();
}

static void execHardware_hstart()
{
	nds.VCount++;

	//end of 3d vblank
	//this should be 214, but we are going to be generous for games with tight timing
	//they shouldnt be changing any textures at 262 but they might accidentally still be at 214
	//so..
	if ( (CommonSettings.rigorous_timing && nds.VCount == 214) || (!CommonSettings.rigorous_timing && nds.VCount == 262) )
	{
		gfx3d_VBlankEndSignal(frameSkipper.ShouldSkip3D());
	}
	
	GPU->UpdateAverageBacklightIntensityTotal();

	if (nds.VCount == 263)
	{
		nds.VCount = 0;
		GPU->SetDisplayCaptureEnable();
	}
	else if (nds.VCount == 262)
	{
		if (!NDS_ARM9.freeze && nds.overclock < 2 && CommonSettings.gamehacks.flags.overclock)
		{
			//suspend arm7 during overclocking so much doesn't run out of control
			//actually, this isn't needed yet.
			//NDS_ARM7.freeze |= CPU_FREEZE_OVERCLOCK_HACK;

			nds.overclock++;
			nds.VCount = 261;
		}
		else
		{
			//overclock arm7 lock is always released here; if it wasn't actiev, this benign
			NDS_ARM7.freeze &= ~CPU_FREEZE_OVERCLOCK_HACK;

			//when the vcount hits 262, vblank ends (oam pre-renders by one scanline)
			execHardware_hstart_vblankEnd();
		}
	}
	else if (nds.VCount == 261)
	{
		nds.overclock = 0;
	}
	else if (nds.VCount == 192)
	{
		GPU->ResetDisplayCaptureEnable();
		
		//turn on vblank status bit
		T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) | 1);
		T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 1);

		//check whether we'll need to fire vblank irqs
		if(T1ReadWord(MMU.ARM9_REG, 4) & 0x8) MMU.reg_IF_pending[ARMCPU_ARM9] |= (1<<IRQ_BIT_LCD_VBLANK);
		if(T1ReadWord(MMU.ARM7_REG, 4) & 0x8) MMU.reg_IF_pending[ARMCPU_ARM7] |= (1<<IRQ_BIT_LCD_VBLANK);

		//this is important for the character select in Dragon Ball Kai - Ultimate Butouden
		//it seems if you allow the 3d to begin before the vblank, then it will get interrupted and not complete.
		//the game needs to pick up the gxstat reg busy as clear after it finishes processing vblank.
		//therefore, this can't happen until sometime after vblank.
		//devil survivor 2 will have screens get stuck if this is on any other scanline.
		//obviously 192 is the right choice.
		gfx3d_VBlankSignal();
		//this isnt important for any known game, but it would be nice to prove it.
		NDS_RescheduleGXFIFO(392*2);
	}

	//write the new vcount
	T1WriteWord(MMU.ARM9_REG, 6, nds.VCount);
	T1WriteWord(MMU.ARM9_REG, 0x1006, nds.VCount);
	T1WriteWord(MMU.ARM7_REG, 6, nds.VCount);
	T1WriteWord(MMU.ARM7_REG, 0x1006, nds.VCount);

	//turn off hblank status bit
	T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) & 0xFFFD);
	T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFD);

	//handle vcount status
	execHardware_hstart_vcount();

	//trigger hstart dmas
	triggerDma(EDMAMode_HStart);

	if (nds.VCount < 192)
	{
		//this is hacky.
		//there is a corresponding hack in doDMA.
		//it should be driven by a fifo (and generate just in time as the scanline is displayed)
		//but that isnt even possible until we have some sort of sub-scanline timing.
		//it may not be necessary.
		triggerDma(EDMAMode_MemDisplay);
	}
}

void NDS_Reschedule()
{
	IF_DEVELOPER(if(!sequencer.reschedule) DEBUG_statistics.sequencerExecutionCounters[0]++;);
	sequencer.reschedule = true;
}

FORCEINLINE u32 _fast_min32(u32 a, u32 b, u32 c, u32 d)
{
	return ((( ((s32)(a-b)) >> (32-1)) & (c^d)) ^ d);
}

FORCEINLINE u64 _fast_min(u64 a, u64 b)
{
	//you might find that this is faster on a 64bit system; someone should try it
	//http://aggregate.org/MAGIC/#Integer%20Selection
	//u64 ret = (((((s64)(a-b)) >> (64-1)) & (a^b)) ^ b);
	//assert(ret==min(a,b));
	//return ret;	
	
	//but this ends up being the fastest on 32bits
	return a<b?a:b;

	//just for the record, I tried to do the 64bit math on a 32bit proc
	//using sse2 and it was really slow
	//__m128i __a; __a.m128i_u64[0] = a;
	//__m128i __b; __b.m128i_u64[0] = b;
	//__m128i xorval = _mm_xor_si128(__a,__b);
	//__m128i temp = _mm_sub_epi64(__a,__b);
	//temp.m128i_i64[0] >>= 63; //no 64bit shra in sse2, what a disappointment
	//temp = _mm_and_si128(temp,xorval);
	//temp = _mm_xor_si128(temp,__b);
	//return temp.m128i_u64[0];
}



u64 Sequencer::findNext()
{
	//this one is always enabled so dont bother to check it
	u64 next = dispcnt.next();

	if(divider.isEnabled()) next = _fast_min(next,divider.next());
	if(sqrtunit.isEnabled()) next = _fast_min(next,sqrtunit.next());
	if(gxfifo.enabled) next = _fast_min(next,gxfifo.next());
	if(readslot1.isEnabled()) next = _fast_min(next,readslot1.next());
	if (wifi.enabled) next = _fast_min(next,wifi.next());

#define test(X,Y) if(dma_##X##_##Y .isEnabled()) next = _fast_min(next,dma_##X##_##Y .next());
	test(0,0); test(0,1); test(0,2); test(0,3);
	test(1,0); test(1,1); test(1,2); test(1,3);
#undef test
#define test(X,Y) if(timer_##X##_##Y .enabled) next = _fast_min(next,timer_##X##_##Y .next());
	test(0,0); test(0,1); test(0,2); test(0,3);
	test(1,0); test(1,1); test(1,2); test(1,3);
#undef test

	return next;
}

void Sequencer::execHardware()
{
	if(dispcnt.isTriggered())
	{

		IF_DEVELOPER(DEBUG_statistics.sequencerExecutionCounters[1]++);

		switch(dispcnt.param)
		{
		case ESI_DISPCNT_HStart:
			execHardware_hstart();
			//(used to be 3168)
			//hstart is actually 8 dots before the visible drawing begins
			//we're going to run 1 here and then run 7 in the next case
			dispcnt.timestamp += 1*6*2;
			dispcnt.param = ESI_DISPCNT_HStartIRQ;
			break;
		case ESI_DISPCNT_HStartIRQ:
			execHardware_hstart_irq();
			dispcnt.timestamp += 7*6*2;
			dispcnt.param = ESI_DISPCNT_HDraw;
			break;
			
		case ESI_DISPCNT_HDraw:
			execHardware_hdraw();
			//duration of non-blanking period is ~1606 clocks (gbatek agrees) [but says its different on arm7]
			//im gonna call this 267 dots = 267*6=1602
			//so, this event lasts 267 dots minus the 8 dot preroll
			dispcnt.timestamp += (267-8)*6*2;
			dispcnt.param = ESI_DISPCNT_HBlank;
			break;

		case ESI_DISPCNT_HBlank:
			execHardware_hblank();
			//(once this was 1092 or 1092/12=91 dots.)
			//there are surely 355 dots per scanline, less 267 for non-blanking period. the rest is hblank and then after that is hstart
			dispcnt.timestamp += (355-267)*6*2;
			dispcnt.param = ESI_DISPCNT_HStart;
			break;
		}
	}

	if (wifiHandler->GetCurrentEmulationLevel() != WifiEmulationLevel_Off)
	{
		if (wifi.isTriggered())
		{
			wifiHandler->CommTrigger();
			wifi.timestamp += kWifiCycles;
		}
	}
	
	if(divider.isTriggered()) divider.exec();
	if(sqrtunit.isTriggered()) sqrtunit.exec();
	if(gxfifo.isTriggered()) gxfifo.exec();
	if(readslot1.isTriggered()) readslot1.exec();


#define test(X,Y) if(dma_##X##_##Y .isTriggered()) dma_##X##_##Y .exec();
	test(0,0); test(0,1); test(0,2); test(0,3);
	test(1,0); test(1,1); test(1,2); test(1,3);
#undef test
#define test(X,Y) if(timer_##X##_##Y .enabled) if(timer_##X##_##Y .isTriggered()) timer_##X##_##Y .exec();
	test(0,0); test(0,1); test(0,2); test(0,3);
	test(1,0); test(1,1); test(1,2); test(1,3);
#undef test
}

void execHardware_interrupts();

static void saveUserInput(EMUFILE &os);
static bool loadUserInput(EMUFILE &is, int version);

void nds_savestate(EMUFILE &os)
{
	//version
	os.write_32LE(4);

	sequencer.save(os);

	saveUserInput(os);

	os.write_32LE(LidClosed);
	os.write_u8(countLid);
}

bool nds_loadstate(EMUFILE &is, int size)
{
	// this isn't part of the savestate loading logic, but
	// don't skip the next frame after loading a savestate
	frameSkipper.OmitSkip(true, true);

	//read version
	u32 version;
	if (is.read_32LE(version) != 1) return false;

	if (version > 4) return false;
	// hacky fix; commit 281268e added to the saved info but didn't update version
	if (version == 3)
	{
		if (size == 497)
			version = 4;
	}

	bool temp = true;
	temp &= sequencer.load(is, version);
	if (version <= 1 || !temp) return temp;
	temp &= loadUserInput(is, version);

	if (version < 3) return temp;

	is.read_32LE(LidClosed);
	is.read_u8(countLid);

	return temp;
}

FORCEINLINE void arm9log()
{
#ifdef LOG_ARM9
	if(dolog)
	{
		char dasmbuf[4096];
		if(NDS_ARM9.CPSR.bits.T)
			des_thumb_instructions_set[((NDS_ARM9.instruction)>>6)&1023](NDS_ARM9.instruct_adr, NDS_ARM9.instruction, dasmbuf);
		else
			des_arm_instructions_set[INDEX(NDS_ARM9.instruction)](NDS_ARM9.instruct_adr, NDS_ARM9.instruction, dasmbuf);

#ifdef LOG_TO_FILE
		if (!fp_dis9) return;
#ifdef LOG_TO_FILE_REGS
		fprintf(fp_dis9, "\t\t;%05d:%03d %12lld\n\t\t;R0:%08X R1:%08X R2:%08X R3:%08X R4:%08X R5:%08X R6:%08X R7:%08X R8:%08X R9:%08X\n\t\t;R10:%08X R11:%08X R12:%08X R13:%08X R14:%08X R15:%08X| next %08X, N:%i Z:%i C:%i V:%i\n",
			currFrameCounter, nds.VCount, nds_timer,
			NDS_ARM9.R[0],  NDS_ARM9.R[1],  NDS_ARM9.R[2],  NDS_ARM9.R[3],  NDS_ARM9.R[4],  NDS_ARM9.R[5],  NDS_ARM9.R[6],  NDS_ARM9.R[7], 
			NDS_ARM9.R[8],  NDS_ARM9.R[9],  NDS_ARM9.R[10],  NDS_ARM9.R[11],  NDS_ARM9.R[12],  NDS_ARM9.R[13],  NDS_ARM9.R[14],  NDS_ARM9.R[15],
			NDS_ARM9.next_instruction, NDS_ARM9.CPSR.bits.N, NDS_ARM9.CPSR.bits.Z, NDS_ARM9.CPSR.bits.C, NDS_ARM9.CPSR.bits.V);
#endif
		fprintf(fp_dis9, "%s %08X\t%08X \t%s\n", NDS_ARM9.CPSR.bits.T?"THUMB":"ARM", NDS_ARM9.instruct_adr, NDS_ARM9.instruction, dasmbuf);
		/*if (NDS_ARM9.instruction == 0)
		{
			dolog = false;
			INFO("Disassembler is stopped\n");
		}*/
#else
		printf("%05d:%03d %12lld 9:%08X %08X %-30s R00:%08X R01:%08X R02:%08X R03:%08X R04:%08X R05:%08X R06:%08X R07:%08X R08:%08X R09:%08X R10:%08X R11:%08X R12:%08X R13:%08X R14:%08X R15:%08X\n",
			currFrameCounter, nds.VCount, nds_timer, 
			NDS_ARM9.instruct_adr,NDS_ARM9.instruction, dasmbuf, 
			NDS_ARM9.R[0],  NDS_ARM9.R[1],  NDS_ARM9.R[2],  NDS_ARM9.R[3],  NDS_ARM9.R[4],  NDS_ARM9.R[5],  NDS_ARM9.R[6],  NDS_ARM9.R[7], 
			NDS_ARM9.R[8],  NDS_ARM9.R[9],  NDS_ARM9.R[10],  NDS_ARM9.R[11],  NDS_ARM9.R[12],  NDS_ARM9.R[13],  NDS_ARM9.R[14],  NDS_ARM9.R[15]);  
#endif
	}
#endif
}

FORCEINLINE void arm7log()
{
#ifdef LOG_ARM7
	if(dolog)
	{
		char dasmbuf[4096];
		if(NDS_ARM7.CPSR.bits.T)
			des_thumb_instructions_set[((NDS_ARM7.instruction)>>6)&1023](NDS_ARM7.instruct_adr, NDS_ARM7.instruction, dasmbuf);
		else
			des_arm_instructions_set[INDEX(NDS_ARM7.instruction)](NDS_ARM7.instruct_adr, NDS_ARM7.instruction, dasmbuf);
#ifdef LOG_TO_FILE
		if (!fp_dis7) return;
#ifdef LOG_TO_FILE_REGS
		fprintf(fp_dis7, "\t\t;%05d:%03d %12lld\n\t\t;R0:%08X R1:%08X R2:%08X R3:%08X R4:%08X R5:%08X R6:%08X R7:%08X R8:%08X R9:%08X\n\t\t;R10:%08X R11:%08X R12:%08X R13:%08X R14:%08X R15:%08X| next %08X, N:%i Z:%i C:%i V:%i\n",
			currFrameCounter, nds.VCount, nds_timer, 
			NDS_ARM7.R[0],  NDS_ARM7.R[1],  NDS_ARM7.R[2],  NDS_ARM7.R[3],  NDS_ARM7.R[4],  NDS_ARM7.R[5],  NDS_ARM7.R[6],  NDS_ARM7.R[7], 
			NDS_ARM7.R[8],  NDS_ARM7.R[9],  NDS_ARM7.R[10],  NDS_ARM7.R[11],  NDS_ARM7.R[12],  NDS_ARM7.R[13],  NDS_ARM7.R[14],  NDS_ARM7.R[15],
			NDS_ARM7.next_instruction, NDS_ARM7.CPSR.bits.N, NDS_ARM7.CPSR.bits.Z, NDS_ARM7.CPSR.bits.C, NDS_ARM7.CPSR.bits.V);
#endif
		fprintf(fp_dis7, "%s %08X\t%08X \t%s\n", NDS_ARM7.CPSR.bits.T?"THUMB":"ARM", NDS_ARM7.instruct_adr, NDS_ARM7.instruction, dasmbuf);
		/*if (NDS_ARM7.instruction == 0)
		{
			dolog = false;
			INFO("Disassembler is stopped\n");
		}*/
#else		
		printf("%05d:%03d %12lld 7:%08X %08X %-30s R00:%08X R01:%08X R02:%08X R03:%08X R04:%08X R05:%08X R06:%08X R07:%08X R08:%08X R09:%08X R10:%08X R11:%08X R12:%08X R13:%08X R14:%08X R15:%08X\n",
			currFrameCounter, nds.VCount, nds_timer, 
			NDS_ARM7.instruct_adr,NDS_ARM7.instruction, dasmbuf, 
			NDS_ARM7.R[0],  NDS_ARM7.R[1],  NDS_ARM7.R[2],  NDS_ARM7.R[3],  NDS_ARM7.R[4],  NDS_ARM7.R[5],  NDS_ARM7.R[6],  NDS_ARM7.R[7], 
			NDS_ARM7.R[8],  NDS_ARM7.R[9],  NDS_ARM7.R[10],  NDS_ARM7.R[11],  NDS_ARM7.R[12],  NDS_ARM7.R[13],  NDS_ARM7.R[14],  NDS_ARM7.R[15]);
#endif
	}
#endif
}

//these have not been tuned very well yet.
static const int kMaxWork = 4000;
static const int kIrqWait = 4000;


template<bool doarm9, bool doarm7>
static FORCEINLINE s32 minarmtime(s32 arm9, s32 arm7)
{
	if(doarm9)
		if(doarm7)
			return min(arm9,arm7);
		else
			return arm9;
	else
		return arm7;
}

#ifdef HAVE_JIT
template<bool doarm9, bool doarm7, bool jit>
#else
template<bool doarm9, bool doarm7>
#endif
static /*donotinline*/ std::pair<s32,s32> armInnerLoop(
	const u64 nds_timer_base, const s32 s32next, s32 arm9, s32 arm7)
{
	s32 timer = minarmtime<doarm9,doarm7>(arm9,arm7);
	while(timer < s32next && !sequencer.reschedule && execute)
	{
		// breakpoint handling
		#if defined(HOST_WINDOWS) && !defined(TARGET_INTERFACE)
		const std::vector<u32> *breakpointList9 = NDS_ARM9.breakPoints;
		for (int i = 0; i < breakpointList9->size(); ++i) {
			if (NDS_ARM9.instruct_adr == (*breakpointList9)[i] && !NDS_ARM9.debugStep) {
				emu_paused = true;
				paused = true;
				execute = false;
				// update debug display
				PostMessageA(DisViewWnd[0], WM_COMMAND, IDC_DISASMSEEK, NDS_ARM9.instruct_adr);
				InvalidateRect(DisViewWnd[0], NULL, FALSE);
				return std::make_pair(arm9, arm7);
			}
		}
		const std::vector<u32> *breakpointList7 = NDS_ARM7.breakPoints;
		for (int i = 0; i < breakpointList7->size(); ++i) {
			if (NDS_ARM7.instruct_adr == (*breakpointList7)[i] && !NDS_ARM7.debugStep) {
				emu_paused = true;
				paused = true;
				execute = false;
				// update debug display
				PostMessageA(DisViewWnd[1], WM_COMMAND, IDC_DISASMSEEK, NDS_ARM7.instruct_adr);
				InvalidateRect(DisViewWnd[1], NULL, FALSE);
				return std::make_pair(arm9, arm7);
			}
		}
		#endif //HOST_WINDOWS

		if(doarm9 && (!doarm7 || arm9 <= timer))
		{
			if(!(NDS_ARM9.freeze & CPU_FREEZE_WAIT_IRQ) && !nds.freezeBus)
			{
				arm9log();
				debug();
#ifdef HAVE_JIT
				arm9 += armcpu_exec<ARMCPU_ARM9,jit>();
#else
				arm9 += armcpu_exec<ARMCPU_ARM9>();
#endif
				#ifdef DEVELOPER
					nds_debug_continuing[0] = false;
				#endif
			}
			else
			{
				s32 temp = arm9;
				arm9 = min(s32next, arm9 + kIrqWait);
				nds.idleCycles[0] += arm9-temp;
				if (gxFIFO.size < 255) nds.freezeBus &= ~1;
			}
			// for proper stepping...
			if (NDS_ARM9.debugStep) {
				NDS_ARM9.debugStep = false;
				execute = false;
				//PostMessageA(DisViewWnd[0], WM_COMMAND, IDC_DISASMSEEK, NDS_ARM9.instruct_adr);
				//InvalidateRect(DisViewWnd[0], NULL, FALSE);
			}
			if (NDS_ARM9.stepOverBreak == NDS_ARM9.instruct_adr && NDS_ARM9.stepOverBreak != 0) {
				NDS_ARM9.stepOverBreak = 0;
				execute = false;
				//PostMessageA(DisViewWnd[0], WM_COMMAND, IDC_DISASMSEEK, NDS_ARM9.instruct_adr);
				//InvalidateRect(DisViewWnd[0], NULL, FALSE);
			}
			// aaand handle step to return
			if (NDS_ARM9.runToRetTmp != 0 && NDS_ARM9.runToRetTmp == NDS_ARM9.instruct_adr) {
				NDS_ARM9.runToRetTmp = 0;
				NDS_ARM9.runToRet = true;
			}
		}
		if(doarm7 && (!doarm9 || arm7 <= timer))
		{
			bool cpufreeze = !!(NDS_ARM7.freeze & (CPU_FREEZE_WAIT_IRQ|CPU_FREEZE_OVERCLOCK_HACK));
			if(!cpufreeze && !nds.freezeBus)
			{
				arm7log();
#ifdef HAVE_JIT
				arm7 += (armcpu_exec<ARMCPU_ARM7,jit>()<<1);
#else
				arm7 += (armcpu_exec<ARMCPU_ARM7>()<<1);
#endif
				#ifdef DEVELOPER
					nds_debug_continuing[1] = false;
				#endif
			}
			else
			{
				s32 temp = arm7;
				arm7 = min(s32next, arm7 + kIrqWait);
				nds.idleCycles[1] += arm7-temp;
				if(arm7 == s32next)
				{
					nds_timer = nds_timer_base + minarmtime<doarm9,false>(arm9,arm7);
#ifdef HAVE_JIT
					return armInnerLoop<doarm9,false,jit>(nds_timer_base, s32next, arm9, arm7);
#else
					return armInnerLoop<doarm9,false>(nds_timer_base, s32next, arm9, arm7);
#endif
				}
			}
			if (NDS_ARM7.debugStep) {
				NDS_ARM7.debugStep = false;
				execute = false;
				//PostMessageA(DisViewWnd[1], WM_COMMAND, IDC_DISASMSEEK, NDS_ARM7.instruct_adr);
				//InvalidateRect(DisViewWnd[1], NULL, FALSE);
			}
			if (NDS_ARM7.stepOverBreak == NDS_ARM7.instruct_adr && NDS_ARM7.stepOverBreak != 0) {
				NDS_ARM7.stepOverBreak = 0;
				execute = false;
				//PostMessageA(DisViewWnd[1], WM_COMMAND, IDC_DISASMSEEK, NDS_ARM7.instruct_adr);
				//InvalidateRect(DisViewWnd[1], NULL, FALSE);
			}
			// aaand handle step to return
			if (NDS_ARM7.runToRetTmp != 0 && NDS_ARM7.runToRetTmp == NDS_ARM7.instruct_adr) {
				NDS_ARM7.runToRetTmp = 0;
				NDS_ARM7.runToRet = true;
			}
		}

		timer = minarmtime<doarm9,doarm7>(arm9,arm7);
		nds_timer = nds_timer_base + timer;
	}

	return std::make_pair(arm9, arm7);
}

void NDS_debug_break()
{
	NDS_ARM9.stalled = NDS_ARM7.stalled = 1;

	//triggers an immediate exit from the cpu loop
	NDS_Reschedule();
}

void NDS_debug_continue()
{
	NDS_ARM9.stalled = NDS_ARM7.stalled = 0;
	execute = true;
}

void NDS_debug_step()
{
	NDS_debug_continue();
	singleStep = true;
}

template<bool FORCE>
void NDS_exec(s32 nb)
{
	GDBSTUB_MUTEX_LOCK();

	LagFrameFlag=1;

	sequencer.nds_vblankEnded = false;

	nds.cpuloopIterationCount = 0;

	IF_DEVELOPER(for(int i=0;i<32;i++) DEBUG_statistics.sequencerExecutionCounters[i] = 0);

	if(nds.sleeping)
	{
		//speculative code: if ANY irq happens, wake up the arm7.
		//I think the arm7 program analyzes the system and may decide not to wake up
		//if it is dissatisfied with the conditions
		if((MMU.reg_IE[1] & MMU.gen_IF<1>()))
		{
			nds.sleeping = FALSE;
		}
	}
	else
	{
		for(;;)
		{
			//trap the debug-stalled condition
			#ifdef DEVELOPER
				singleStep = false;
				//(gdb stub doesnt yet know how to trigger these immediately by calling reschedule)
				if ((NDS_ARM9.stalled || NDS_ARM7.stalled) && execute)
				{
					driver->EMU_DebugIdleEnter();

					while((NDS_ARM9.stalled || NDS_ARM7.stalled) && execute)
					{
						GDBSTUB_MUTEX_UNLOCK();
						driver->EMU_DebugIdleUpdate();
						GDBSTUB_MUTEX_LOCK();
						nds_debug_continuing[0] = nds_debug_continuing[1] = true;
					}

					driver->EMU_DebugIdleWakeUp();
				}
			#endif

			nds.cpuloopIterationCount++;
			sequencer.execHardware();

			//break out once per frame
			if(sequencer.nds_vblankEnded) break;
			//it should be benign to execute execHardware in the next frame,
			//since there won't be anything for it to do (everything should be scheduled in the future)

			//bail in case the system halted
			if(!execute) break;

			execHardware_interrupts();

			//find next work unit:
			u64 next = sequencer.findNext();
			next = min(next,nds_timer+kMaxWork); //lets set an upper limit for now

			//printf("%d\n",(next-nds_timer));

			sequencer.reschedule = false;

			//cast these down to 32bits so that things run faster on 32bit procs
			u64 nds_timer_base = nds_timer;
			s32 arm9 = (s32)(nds_arm9_timer-nds_timer);
			s32 arm7 = (s32)(nds_arm7_timer-nds_timer);
			s32 s32next = (s32)(next-nds_timer);

			#ifdef DEVELOPER
				if(singleStep)
				{
					s32next = 1;
				}
			#endif

#ifdef HAVE_JIT
			std::pair<s32,s32> arm9arm7 = CommonSettings.use_jit
				? armInnerLoop<true,true,true>(nds_timer_base,s32next,arm9,arm7)
				: armInnerLoop<true,true,false>(nds_timer_base,s32next,arm9,arm7);
#else
				std::pair<s32,s32> arm9arm7 = armInnerLoop<true,true>(nds_timer_base,s32next,arm9,arm7);
#endif

			#ifdef DEVELOPER
				if(singleStep)
				{
					NDS_ARM9.stalled = NDS_ARM7.stalled = 1;
				}
			#endif

			arm9 = arm9arm7.first;
			arm7 = arm9arm7.second;
			nds_arm7_timer = nds_timer_base+arm7;
			nds_arm9_timer = nds_timer_base+arm9;

#ifndef NDEBUG
			//what we find here is dependent on the timing constants above
			//if(nds_timer>next && (nds_timer-next)>22)
			//	printf("curious. please report: over by %d\n",(int)(nds_timer-next));
#endif

			//if we were waiting for an irq, don't wait too long:
			//let's re-analyze it after this hardware event (this rolls back a big burst of irq waiting which may have been interrupted by a resynch)
			if(NDS_ARM9.freeze & CPU_FREEZE_WAIT_IRQ)
			{
				nds.idleCycles[0] -= (s32)(nds_arm9_timer-nds_timer);
				nds_arm9_timer = nds_timer;
			}
			if(NDS_ARM7.freeze & CPU_FREEZE_WAIT_IRQ)
			{
				nds.idleCycles[1] -= (s32)(nds_arm7_timer-nds_timer);
				nds_arm7_timer = nds_timer;
			}
		}
	}

	//DEBUG_statistics.printSequencerExecutionCounters();
	//DEBUG_statistics.print();

	//end of frame emulation housekeeping
	if(LagFrameFlag)
	{
		lagframecounter++;
		TotalLagFrames++;
	}
	else 
	{
		lastLag = lagframecounter;
		lagframecounter = 0;
	}
	currFrameCounter++;
	DEBUG_Notify.NextFrame();
	if(cheats) cheats->process(CHEAT_TYPE_INTERNAL);

	GDBSTUB_MUTEX_UNLOCK();
}

template<int PROCNUM> static void execHardware_interrupts_core()
{
	u32 IF = MMU.gen_IF<PROCNUM>();
	u32 IE = MMU.reg_IE[PROCNUM];
	u32 masked = IF & IE;
	if((ARMPROC.freeze & CPU_FREEZE_IRQ_IE_IF) && masked)
	{
		ARMPROC.freeze &= ~CPU_FREEZE_IRQ_IE_IF;
	}

	if(masked && MMU.reg_IME[PROCNUM] && !ARMPROC.CPSR.bits.I)
	{
		//printf("Executing IRQ on procnum %d with IF = %08X and IE = %08X\n",PROCNUM,IF,IE);
		armcpu_irqException(&ARMPROC);
	}
}

void execHardware_interrupts()
{
	execHardware_interrupts_core<ARMCPU_ARM9>();
	execHardware_interrupts_core<ARMCPU_ARM7>();
}

static void resetUserInput();

static void PrepareBiosARM7()
{
	//begin with the bios unloaded
	NDS_ARM7.BIOS_loaded = false;
	memset(MMU.ARM7_BIOS, 0, sizeof(MMU.ARM7_BIOS));

	if(CommonSettings.UseExtBIOS)
	{
		//read arm7 bios from inputfile and flag it if it succeeds
		FILE *arm7inf = fopen(CommonSettings.ARM7BIOS,"rb");
		if (arm7inf) 
		{
			if (fread(MMU.ARM7_BIOS,1,16384,arm7inf) == 16384)
				NDS_ARM7.BIOS_loaded = true;
			fclose(arm7inf);
		}
	}

	//choose to use SWI emulation or routines from bios
	if((CommonSettings.SWIFromBIOS) && (NDS_ARM7.BIOS_loaded))
	{
		NDS_ARM7.swi_tab = 0;

		//if we used routines from bios, apply patches
		if (CommonSettings.PatchSWI3)
		{
			//[3801] SUB R0, #1 -> [4770] BX LR
			T1WriteWord(MMU.ARM7_BIOS,0x2F08, 0x4770);
		}
	} 
	else 
		NDS_ARM7.swi_tab = ARM_swi_tab[ARMCPU_ARM7];

	if(NDS_ARM7.BIOS_loaded)
	{
		INFO("ARM7 BIOS load: %s.\n", NDS_ARM7.BIOS_loaded?"OK":"FAILED");
	}
	else
	{
		//fake bios content, critical to normal operations, since we dont have a real bios.

		T1WriteLong(MMU.ARM7_BIOS, 0x0000, 0xEAFFFFFE); //B 00000000 (reset: infinite loop) (originally: 0xE25EF002 - SUBS PC, LR, #2  
		T1WriteLong(MMU.ARM7_BIOS, 0x0004, 0xEAFFFFFE); //B 00000004 (undefined instruction: infinite loop)
		T1WriteLong(MMU.ARM7_BIOS, 0x0008, 0xEAFFFFFE); //B 00000280 (SWI: infinite loop [since we will be HLEing the SWI routines])
		T1WriteLong(MMU.ARM7_BIOS, 0x000C, 0xEAFFFFFE); //B 0000000C (prefetch abort: infinite loop)
		T1WriteLong(MMU.ARM7_BIOS, 0x0010, 0xEAFFFFFE); //B 00000010 (data abort: infinite loop)
		T1WriteLong(MMU.ARM7_BIOS, 0x0018, 0xEA000000); //B 00000020 (IRQ: branch to handler)
		T1WriteLong(MMU.ARM7_BIOS, 0x001C, 0xEAFFFFFE); //B 0000001C (FIQ vector: infinite loop)
		//IRQ handler
		T1WriteLong(MMU.ARM7_BIOS, 0x0020, 0xE92D500F); //STMDB SP!, {R0-R3,R12,LR}
		T1WriteLong(MMU.ARM7_BIOS, 0x0024, 0xE3A00301); //MOV R0, #4000000
		T1WriteLong(MMU.ARM7_BIOS, 0x0028, 0xE28FE000); //ADD LR, PC, #0
		T1WriteLong(MMU.ARM7_BIOS, 0x002C, 0xE510F004); //LDR PC, [R0, -#4]
		T1WriteLong(MMU.ARM7_BIOS, 0x0030, 0xE8BD500F); //LDMIA SP!, {R0-R3,R12,LR}
		T1WriteLong(MMU.ARM7_BIOS, 0x0034, 0xE25EF004); //SUBS PC, LR, #4
	}
}

static void PrepareBiosARM9()
{
	//begin with the bios unloaded
	memset(MMU.ARM9_BIOS, 0, sizeof(MMU.ARM9_BIOS));
	NDS_ARM9.BIOS_loaded = false;

	if(CommonSettings.UseExtBIOS)
	{
		//read arm9 bios from inputfile and flag it if it succeeds
		FILE* arm9inf = fopen(CommonSettings.ARM9BIOS,"rb");
		if (arm9inf) 
		{
			if (fread(MMU.ARM9_BIOS,1,4096,arm9inf) == 4096) 
				NDS_ARM9.BIOS_loaded = true;
			fclose(arm9inf);
		}
	}

	//choose to use SWI emulation or routines from bios
	if((CommonSettings.SWIFromBIOS) && (NDS_ARM9.BIOS_loaded))
	{
		NDS_ARM9.swi_tab = 0;
		
		//if we used routines from bios, apply patches
		//[3801] SUB R0, #1 -> [4770] BX LR
		if (CommonSettings.PatchSWI3)
			T1WriteWord(MMU.ARM9_BIOS, 0x07CC, 0x4770);
	}
	else NDS_ARM9.swi_tab = ARM_swi_tab[ARMCPU_ARM9];

	if(NDS_ARM9.BIOS_loaded) 
	{
		INFO("ARM9 BIOS load: %s.\n", NDS_ARM9.BIOS_loaded?"OK":"FAILED");
	} 
	else 
	{
		//fake bios content, critical to normal operations, since we dont have a real bios.
		//it'd be cool if we could write this in some kind of assembly language, inline or otherwise, without some bulky dependencies
		//perhaps we could build it with devkitarm? but thats bulky (offline) dependencies, to be sure..

		//reminder: bios chains data abort to fast irq

		//exception vectors:
		T1WriteLong(MMU.ARM9_BIOS, 0x0000, 0xEAFFFFFE);		// (infinite loop for) Reset !!!
		//T1WriteLong(MMU.ARM9_BIOS, 0x0004, 0xEAFFFFFE);		// (infinite loop for) Undefined instruction
		T1WriteLong(MMU.ARM9_BIOS, 0x0004, 0xEA000004);		// Undefined instruction -> Fast IRQ (just guessing)
		T1WriteLong(MMU.ARM9_BIOS, 0x0008, 0xEA00009C);		// SWI -> ?????
		T1WriteLong(MMU.ARM9_BIOS, 0x000C, 0xEAFFFFFE);		// (infinite loop for) Prefetch Abort
		T1WriteLong(MMU.ARM9_BIOS, 0x0010, 0xEA000001);		// Data Abort -> Fast IRQ
		T1WriteLong(MMU.ARM9_BIOS, 0x0014, 0x00000000);		// Reserved
		T1WriteLong(MMU.ARM9_BIOS, 0x0018, 0xEA000095);		// Normal IRQ -> 0x0274
		T1WriteLong(MMU.ARM9_BIOS, 0x001C, 0xEA00009D);		// Fast IRQ -> 0x0298

		//copy the logo content into the bios - Pokemon Platinum uses this in Pal Park trade
		//it compares the logo from the arm9 bios to the logo in the GBA header.
		//NOTE: in the unlikely event that the valid header is missing from the gameInfo, we'd be doing wrong here.
		//      however, its nice not to have the logo embedded here. 
		//      TODO - take a CRC of the logo, check vs logoCRC16, and a hardcoded value, to make sure all is in order--report error if not
		memcpy(&MMU.ARM9_BIOS[0x20], &gameInfo.header.logo[0], 0x9C);
		T1WriteWord(MMU.ARM9_BIOS, 0x20 + 0x9C,  gameInfo.header.logoCRC16);
		//... and with that we are at 0xBC:

		//(now what goes in this gap?? nothing we need, i guess)

		//IRQ handler: get dtcm address and jump to a vector in it
		T1WriteLong(MMU.ARM9_BIOS, 0x0274, 0xE92D500F); //STMDB SP!, {R0-R3,R12,LR} 
		T1WriteLong(MMU.ARM9_BIOS, 0x0278, 0xEE190F11); //MRC CP15, 0, R0, CR9, CR1, 0
		T1WriteLong(MMU.ARM9_BIOS, 0x027C, 0xE1A00620); //MOV R0, R0, LSR #C
		T1WriteLong(MMU.ARM9_BIOS, 0x0280, 0xE1A00600); //MOV R0, R0, LSL #C 
		T1WriteLong(MMU.ARM9_BIOS, 0x0284, 0xE2800C40); //ADD R0, R0, #4000
		T1WriteLong(MMU.ARM9_BIOS, 0x0288, 0xE28FE000); //ADD LR, PC, #0   
		T1WriteLong(MMU.ARM9_BIOS, 0x028C, 0xE510F004); //LDR PC, [R0, -#4] 

		//????
		T1WriteLong(MMU.ARM9_BIOS, 0x0290, 0xE8BD500F); //LDMIA SP!, {R0-R3,R12,LR}
		T1WriteLong(MMU.ARM9_BIOS, 0x0294, 0xE25EF004); //SUBS PC, LR, #4

		//-------
		//FIQ and abort exception handler
		//TODO - this code is copied from the bios. refactor it
		//friendly reminder: to calculate an immediate offset: encoded = (desired_address-cur_address-8)

		T1WriteLong(MMU.ARM9_BIOS, 0x0298, 0xE10FD000); //MRS SP, CPSR  
		T1WriteLong(MMU.ARM9_BIOS, 0x029C, 0xE38DD0C0); //ORR SP, SP, #C0
		
		T1WriteLong(MMU.ARM9_BIOS, 0x02A0, 0xE12FF00D); //MSR CPSR_fsxc, SP
		T1WriteLong(MMU.ARM9_BIOS, 0x02A4, 0xE59FD000 | (0x2D4-0x2A4-8)); //LDR SP, [FFFF02D4]
		T1WriteLong(MMU.ARM9_BIOS, 0x02A8, 0xE28DD001); //ADD SP, SP, #1   
		T1WriteLong(MMU.ARM9_BIOS, 0x02AC, 0xE92D5000); //STMDB SP!, {R12,LR}
		
		T1WriteLong(MMU.ARM9_BIOS, 0x02B0, 0xE14FE000); //MRS LR, SPSR
		T1WriteLong(MMU.ARM9_BIOS, 0x02B4, 0xEE11CF10); //MRC CP15, 0, R12, CR1, CR0, 0
		T1WriteLong(MMU.ARM9_BIOS, 0x02B8, 0xE92D5000); //STMDB SP!, {R12,LR}
		T1WriteLong(MMU.ARM9_BIOS, 0x02BC, 0xE3CCC001); //BIC R12, R12, #1

		T1WriteLong(MMU.ARM9_BIOS, 0x02C0, 0xEE01CF10); //MCR CP15, 0, R12, CR1, CR0, 0
		T1WriteLong(MMU.ARM9_BIOS, 0x02C4, 0xE3CDC001); //BIC R12, SP, #1    
		T1WriteLong(MMU.ARM9_BIOS, 0x02C8, 0xE59CC010); //LDR R12, [R12, #10] 
		T1WriteLong(MMU.ARM9_BIOS, 0x02CC, 0xE35C0000); //CMP R12, #0  

		T1WriteLong(MMU.ARM9_BIOS, 0x02D0, 0x112FFF3C); //BLXNE R12    
		T1WriteLong(MMU.ARM9_BIOS, 0x02D4, 0x027FFD9C); //0x027FFD9C  
		//---------
	}
}

static void JumbleMemory()
{
	//put random garbage in vram for homebrew games, to help mimic the situation where libnds does not clear out junk
	//which the card's launcher may or may not have left behind
	//analysis:
  //1. retail games dont clear TCM, so why should we jumble it and expect homebrew to clear it?
  //2. some retail games _dont boot_ if main memory is jumbled. wha...?
  //3. clearing this is not as useful as tracking uninitialized reads in dev+ builds
  //4. the vram clearing causes lots of graphical corruptions in badly coded homebrews. this reduces compatibility substantially
  //conclusion: disable it for now and bring it back as an option
	//if(gameInfo.isHomebrew)
	//{
	//	u32 w=100000,x=99,y=117,z=19382173;
	//	CTASSERT(sizeof(MMU.ARM9_LCD) < sizeof(MMU.MAIN_MEM));
	//	CTASSERT(sizeof(MMU.ARM9_VMEM) < sizeof(MMU.MAIN_MEM));
	//	CTASSERT(sizeof(MMU.ARM9_ITCM) < sizeof(MMU.MAIN_MEM));
	//	CTASSERT(sizeof(MMU.ARM9_DTCM) < sizeof(MMU.MAIN_MEM));
	//	for(int i=0;i<sizeof(MMU.MAIN_MEM);i++)
	//	{
	//		u32 t= (x^(x<<11)); 
	//		x=y;
	//		y=z; 
	//		z=w;
	//		t = (w= (w^(w>>19))^(t^(t>>8)));
	//		//MMU.MAIN_MEM[i] = t;
	//		if (i<sizeof(MMU.ARM9_LCD)) MMU.ARM9_LCD[i] = t;
	//		if (i<sizeof(MMU.ARM9_VMEM)) MMU.ARM9_VMEM[i] = t;
	//		//if (i<sizeof(MMU.ARM9_ITCM)) MMU.ARM9_ITCM[i] = t;
	//		//if (i<sizeof(MMU.ARM9_DTCM)) MMU.ARM9_DTCM[i] = t;
	//	}
	//}
}

static void PrepareLogfiles()
{
#ifdef LOG_ARM7
	if (fp_dis7 != NULL) 
	{
		fclose(fp_dis7);
		fp_dis7 = NULL;
	}
	fp_dis7 = fopen("D:\\desmume_dis7.asm", "w");
#endif

#ifdef LOG_ARM9
	if (fp_dis9 != NULL) 
	{
		fclose(fp_dis9);
		fp_dis9 = NULL;
	}
	fp_dis9 = fopen("D:\\desmume_dis9.asm", "w");
#endif
}

bool NDS_LegitBoot()
{
	#ifdef HAVE_JIT
		//hack for firmware boot in JIT mode.
		//we know that it takes certain jit parameters to successfully boot the firmware.
		//CRAZYMAX: is it safe to accept anything smaller than 12?
		CommonSettings.jit_max_block_size = std::min(CommonSettings.jit_max_block_size,12U);
	#endif
	
	//since firmware only boots encrypted roms, we have to make sure it's encrypted first
	//this has not been validated on big endian systems. it almost positively doesn't work.
	if (gameInfo.header.CRC16 != 0)
		EncryptSecureArea((u8*)&gameInfo.header, (u8*)gameInfo.secureArea);

	//boot processors from their bios entrypoints
	armcpu_init(&NDS_ARM7, 0x00000000);
	armcpu_init(&NDS_ARM9, 0xFFFF0000);

	return true;
}

//the fake firmware boot-up process
bool NDS_FakeBoot()
{
	NDS_header * header = NDS_getROMHeader();

	//DEBUG_reset();

	if (!header) return false;

	nds.isFakeBooted = true;

	//since we're bypassing the code to decrypt the secure area, we need to make sure its decrypted first
	//this has not been validated on big endian systems. it almost positively doesn't work.
	if (gameInfo.header.CRC16 != 0)
	{
		bool okRom = DecryptSecureArea((u8*)&gameInfo.header, (u8*)gameInfo.secureArea);

		if(!okRom) {
			printf("Specified file is not a valid rom\n");
			return false;
		}
	}
	
	//firmware loads the game card arm9 and arm7 programs as specified in rom header
	{
		bool hasSecureArea = ((gameInfo.romType == ROM_NDS) && (gameInfo.header.CRC16 != 0));
		//copy the arm9 program to the address specified by rom header
		u32 src = header->ARM9src;
		u32 dst = header->ARM9cpy;
		for(u32 i = 0; i < header->ARM9binSize; i+=4)
		{
			u32 tmp = (hasSecureArea && ((src >= 0x4000) && (src < 0x8000)))?LE_TO_LOCAL_32(*(u32*)(gameInfo.secureArea + (src - 0x4000))):gameInfo.readROM(src);

			_MMU_write32<ARMCPU_ARM9>(dst, tmp);

			dst += 4;
			src += 4;
		}

		//copy the arm7 program to the address specified by rom header
		src = header->ARM7src;
		dst = header->ARM7cpy;
		for(u32 i = 0; i < header->ARM7binSize; i+=4)
		{
			_MMU_write32<ARMCPU_ARM7>(dst, gameInfo.readROM(src));

			dst += 4;
			src += 4;
		}
	}
	
	//bios does this (thats weird, though. shouldnt it get changed when the card is swapped in the firmware menu?
	//right now our firmware menu isnt detecting any change to the card.
	//are some games depending on it being written here? please document.
	//_MMU_write16<ARMCPU_ARM9>(0x027FF808, T1ReadWord(MMU.CART_ROM, 0x15E));

	//firmware sets up a copy of the firmware user settings in memory.
	//TBD - this code is really clunky
	//it seems to be copying the MMU.fw.data data into RAM in the user memory stash locations
	u8 temp_buffer[NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT];
	if ( copy_firmware_user_data( temp_buffer, MMU.fw.data._raw)) {
		for ( int fw_index = 0; fw_index < NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT; fw_index++)
			_MMU_write08<ARMCPU_ARM9>(0x027FFC80 + fw_index, temp_buffer[fw_index]);
	}

	//something copies the whole header to Main RAM 0x27FFE00 on startup. (http://nocash.emubase.de/gbatek.htm#dscartridgeheader)
	//once upon a time this copied 0x90 more. this was thought to be wrong, and changed.
	if(nds.Is_DSI())
	{
		//dsi needs this copied later in memory. there are probably a number of things that  get copied to a later location in memory.. thats where the NDS consoles tend to stash stuff.
		for (int i = 0; i < (0x170); i+=4)
			_MMU_write32<ARMCPU_ARM9>(0x027FFE00 + i, gameInfo.readROM(i));
	}
	else
	{
		for (int i = 0; i < (0x170); i+=4)
			_MMU_write32<ARMCPU_ARM9>(0x027FFE00 + i, gameInfo.readROM(i));
	}

	//the firmware will be booting to these entrypoint addresses via BX (well, the arm9 at least; is unverified for the arm7)
	armcpu_init(&NDS_ARM7, header->ARM7exe);
	armcpu_init(&NDS_ARM9, header->ARM9exe);

	//firmware sets REG_POSTFLG to the value indicating post-firmware status
	MMU.ARM9_REG[0x300] = 1;
	MMU.ARM7_REG[0x300] = 1;

	//firmware makes system think it's booted from card -- EXTREMELY IMPORTANT!!! This is actually checked by some things. (which things?) Thanks to cReDiAr
	_MMU_write08<ARMCPU_ARM7>(0x02FFFC40,0x1); //<zero> removed redundant write to ARM9, this is going to shared main memory. But one has to wonder why r3478 was made which corrected a typo resulting in only ARMCPU_ARM7 getting used.

	//the chipId is read several times
	//for some reason, each of those reads get stored here.
	_MMU_write32<ARMCPU_ARM7>(0x027FF800, gameInfo.chipID);		//1st chipId
	_MMU_write32<ARMCPU_ARM7>(0x027FF804, gameInfo.chipID);		//2nd (secure) chipId
	_MMU_write32<ARMCPU_ARM7>(0x027FFC00, gameInfo.chipID);		//3rd (secure) chipId
	
	// Write the header checksum to memory
	_MMU_write16<ARMCPU_ARM9>(0x027FF808, gameInfo.header.headerCRC16);

	//bitbox 4k demo is so stripped down it relies on default stack values
	//otherwise the arm7 will crash before making a sound
	//(these according to gbatek softreset bios docs)
	NDS_ARM7.R13_svc = 0x0380FFDC;
	NDS_ARM7.R13_irq = 0x0380FFB0;
	NDS_ARM7.R13_usr = 0x0380FF00;
	NDS_ARM7.R[13] = NDS_ARM7.R13_usr;
	//and let's set these for the arm9 while we're at it, though we have no proof
	NDS_ARM9.R13_svc = 0x00803FC0;
	NDS_ARM9.R13_irq = 0x00803FA0;
	NDS_ARM9.R13_usr = 0x00803EC0;
	NDS_ARM9.R13_abt = NDS_ARM9.R13_usr; //????? 
	//I think it is wrong to take gbatek's "SYS" and put it in USR--maybe USR doesnt matter. 
	//i think SYS is all the misc modes. please verify by setting nonsensical stack values for USR here
	NDS_ARM9.R[13] = NDS_ARM9.R13_usr;
	//n.b.: im not sure about all these, I dont know enough about arm9 svc/irq/etc modes
	//and how theyre named in desmume to match them up correctly. i just guessed.

	//--------------------------------
	//setup the homebrew argv
	//this is useful for nitrofs apps which are emulating themselves via cflash
	//struct __argv {
	//	int argvMagic;		//!< argv magic number, set to 0x5f617267 ('_arg') if valid 
	//	char *commandLine;	//!< base address of command line, set of null terminated strings
	//	int length;			//!< total length of command line
	//	int argc;			//!< internal use, number of arguments
	//	char **argv;		//!< internal use, argv pointer
	//};
	std::string rompath = "fat:/" + path.RomName;
	const u32 kCommandline = 0x027E0000;
	//const u32 kCommandline = 0x027FFF84;

	//homebrew-related stuff.
	//its safe to put things in this position.. apparently nothing important is here.
	//however, some games could be checking them as an anti-desmume measure, so we might have to control it with slot-1 settings to suggest booting a homebrew app
	//perhaps we could automatically boot homebrew to an R4-like device.
	_MMU_write32<ARMCPU_ARM9>(0x02FFFE70, 0x5f617267);
	_MMU_write32<ARMCPU_ARM9>(0x02FFFE74, kCommandline); //(commandline starts here)
	_MMU_write32<ARMCPU_ARM9>(0x02FFFE78, rompath.size()+1);
	//0x027FFF7C (argc)
	//0x027FFF80 (argv)
	for(size_t i=0;i<rompath.size();i++)
		_MMU_write08<ARMCPU_ARM9>(kCommandline+i, rompath[i]);
	_MMU_write08<ARMCPU_ARM9>(kCommandline+rompath.size(), 0);
	//--------------------------------

	//Call the card post_fakeboot hook to perform additional initialization
	slot1_device->post_fakeboot(ARMCPU_ARM9);
	slot1_device->post_fakeboot(ARMCPU_ARM7);

	delete header;
	return true;
}

static void NDS_CurrentCPUInfoToNDSError(NDSError &ndsError)
{
	ndsError.programCounterARM9		= NDS_ARM9.R[15];
	ndsError.instructionARM9		= NDS_ARM9.instruction;
	ndsError.instructionAddrARM9	= NDS_ARM9.instruct_adr;
	ndsError.programCounterARM7		= NDS_ARM7.R[15];
	ndsError.instructionARM7		= NDS_ARM7.instruction;
	ndsError.instructionAddrARM7	= NDS_ARM7.instruct_adr;
}

bool _HACK_DONT_STOPMOVIE = false;
void NDS_Reset()
{
	//do nothing if nothing is loaded
	if(lastRom.filename.size()==0)
		return;

	UnloadMovieEmulationSettings();

	//reload last paths if needed
	if(!gameInfo.reader)
	{
		LastRom stash = lastRom;
		NDS_LoadROM(stash.filename.c_str(), stash.physicalName.c_str(), stash.logicalFilename.c_str());
		//yeah, great. LoadROM calls NDS_Reset. Geeze.
		return;
	}

	PrepareLogfiles();

	CommonSettings.gamehacks.apply();

	if(movieMode != MOVIEMODE_INACTIVE && !_HACK_DONT_STOPMOVIE)
		movie_reset_command = true;

	if(movieMode == MOVIEMODE_INACTIVE) {
		currFrameCounter = 0;
		lagframecounter = 0;
		LagFrameFlag = 0;
		lastLag = 0;
		TotalLagFrames = 0;
	}

	resetUserInput();

	
	singleStep = false;
	nds_debug_continuing[0] = nds_debug_continuing[1] = false;
	nds.sleeping = FALSE;
	nds.cardEjected = FALSE;
	nds.freezeBus = 0;
	nds.power1.lcd = nds.power1.gpuMain = nds.power1.gfx3d_render = nds.power1.gfx3d_geometry = nds.power1.gpuSub = nds.power1.dispswap = 1; //is this proper?
	nds.power_geometry = nds.power_render = TRUE; //whether this is proper follows from prior
	nds.power2.speakers = 1;
	nds.power2.wifi = 0;
	nds.wifiCycle = 0;
	memset(nds.timerCycle, 0, sizeof(u64) * 2 * 4);
	nds.old = 0;
	nds.scr_touchX = nds.scr_touchY = nds.adc_touchX = nds.adc_touchY = 0;
	nds.isTouch = 0;
	nds.isFakeBooted = false;
	nds.paddle = 0;
	nds.overclock = 0;
	nds.ConsoleType = CommonSettings.ConsoleType;
	nds._DebugConsole = CommonSettings.DebugConsole;
	nds.ensataEmulation = CommonSettings.EnsataEmulation;
	nds.ensataHandshake = ENSATA_HANDSHAKE_none;
	nds.ensataIpcSyncCounter = 0;
	nds_timer = 0;
	nds_arm9_timer = 0;
	nds_arm7_timer = 0;
	LidClosed = FALSE;
	countLid = 0;
	MicSampleSelection = 0;

	MMU_Reset();
	SetupMMU(nds.Is_DebugConsole(),nds.Is_DSI());
	JumbleMemory();

	#ifdef HAVE_JIT
		arm_jit_reset(CommonSettings.use_jit);
	#endif


	//initialize CP15 specially for this platform
	//TODO - how much of this is necessary for firmware boot?
	//(only ARM9 has CP15)
	armcp15_init(&cp15);
	MMU.ARM9_RW_MODE = BIT7(cp15.ctrl);
	NDS_ARM9.intVector = 0xFFFF0000 * (BIT13(cp15.ctrl));
	NDS_ARM9.LDTBit = !BIT15(cp15.ctrl); //TBit

	PrepareBiosARM7();
	PrepareBiosARM9();

	if (extFirmwareObj)
	{
		delete extFirmwareObj;
		extFirmwareObj = NULL;
	}
	
	bool didLoadExtFirmware = false;
	bool willBootFromFirmware = false;
	bool bootResult = false;
	
	extFirmwareObj = new CFIRMWARE();
	
	// First, load the firmware from an external file if requested.
	if (CommonSettings.UseExtFirmware && NDS_ARM7.BIOS_loaded && NDS_ARM9.BIOS_loaded)
	{
		didLoadExtFirmware = extFirmwareObj->load(CommonSettings.ExtFirmwarePath);
		
		// We will allow a proper firmware boot, if:
		// 1. we have the ARM7 and ARM9 bioses (its doubtful that our HLE bios implement the necessary functions)
		// 2. firmware is available
		// 3. user has requested booting from firmware
		willBootFromFirmware = (CommonSettings.BootFromFirmware && didLoadExtFirmware);
	}
	
	// If we're doing a fake boot, then we must ensure that this value gets set before any firmware settings are changed.
	if (!willBootFromFirmware)
	{
		//bios (or firmware) sets this default, which is generally not important for retail games but some homebrews are depending on
		_MMU_write08<ARMCPU_ARM9>(REG_WRAMCNT,3);
	}
	
	if (didLoadExtFirmware)
	{
		// what is the purpose of unpack?
		extFirmwareObj->unpack();
	}
	else
	{
		// If we didn't successfully load firmware from somewhere, then we need to use
		// our own internal firmware as a stand-in.
		NDS_InitDefaultFirmware(&MMU.fw.data);
	}
	
	// Load the firmware settings.
	if (CommonSettings.UseExtFirmwareSettings && didLoadExtFirmware)
	{
		// Partially clobber the loaded firmware with user settings from the .dfc file.
		std::string extFWUserSettingsString = CFIRMWARE::GetUserSettingsFilePath(CommonSettings.ExtFirmwarePath);
		strncpy(CommonSettings.ExtFirmwareUserSettingsPath, extFWUserSettingsString.c_str(), MAX_PATH);
		
		extFirmwareObj->loadSettings(CommonSettings.ExtFirmwareUserSettingsPath);
	}
	else
	{
		// Otherwise, just use our version of the firmware config.
		NDS_ApplyFirmwareSettingsWithConfig(&MMU.fw.data, CommonSettings.fwConfig);
	}
	
	// Finally, boot the firmware.
	if (willBootFromFirmware)
	{
		bootResult = NDS_LegitBoot();
	}
	else
	{
		bootResult = NDS_FakeBoot();
	}
	
	// Init calibration info
	memcpy(&TSCal, extFirmwareObj->getTouchCalibrate(), sizeof(TSCalInfo));

	GPU->Reset();

	wifiHandler->Reset();
	wifiHandler->CommStart();

	SPU_DeInit();
	SPU_ReInit(!willBootFromFirmware && bootResult);

	//this needs to happen last, pretty much, since it establishes the correct scheduling state based on all of the above initialization
	initSchedule();
	
	_lastNDSError.code = NDSError_NoError;
	_lastNDSError.tag = NDSErrorTag_None;
	NDS_CurrentCPUInfoToNDSError(_lastNDSError);
}

static std::string MakeInputDisplayString(u16 pad, const std::string* Buttons, int count) {
    std::string s;
    for (int x = 0; x < count; x++) {
        if (pad & (1 << x))
            s.append(Buttons[x].size(), ' '); 
        else
            s += Buttons[x];
    }
    return s;
}

static std::string MakeInputDisplayString(u16 pad, u16 padExt) {
    const std::string Buttons[] = {"A", "B", "Sl", "St", "R", "L", "U", "D", "Rs", "Ls"};
    const std::string Ext[] = {"X", "Y"};

    std::string s = MakeInputDisplayString(pad, Ext, ARRAY_SIZE(Ext));
    s += MakeInputDisplayString(padExt, Buttons, ARRAY_SIZE(Buttons));

    return s;
}


buttonstruct<bool> Turbo;
buttonstruct<u32> TurboTime;
buttonstruct<bool> AutoHold;

void ClearAutoHold(void) {
	
	for (u32 i=0; i < ARRAY_SIZE(AutoHold.array); i++) {
		AutoHold.array[i]=false;
	}
}

//convert a 12.4 screen coordinate to an ADC value.
//the desmume host system will track the screen coordinate, but the hardware should be receiving the raw ADC values.
//so we'll need to use this to simulate the ADC values corresponding to the desired screen coords, based on the current TSC calibrations
u16 NDS_getADCTouchPosX(int scrX_lsl4)
{
	scrX_lsl4 >>= 4;
	int rv = ((scrX_lsl4 - TSCal.scr.x1 + 1) * TSCal.adc.width) / TSCal.scr.width + TSCal.adc.x1;
	rv = min(0xFFF, max(0, rv));
	return (u16)(rv);
}
u16 NDS_getADCTouchPosY(int scrY_lsl4)
{
	scrY_lsl4 >>= 4;
	int rv = ((scrY_lsl4 - TSCal.scr.y1 + 1) * TSCal.adc.height) / TSCal.scr.height + TSCal.adc.y1;
	rv = min(0xFFF, max(0, rv));
	return (u16)(rv);
}

static UserInput rawUserInput = {}; // requested input, generally what the user is physically pressing
static UserInput intermediateUserInput = {}; // intermediate buffer for modifications (seperated from finalUserInput for safety reasons)
static UserInput finalUserInput = {}; // what gets sent to the game and possibly recorded
bool validToProcessInput = false;

const UserInput& NDS_getRawUserInput()
{
	return rawUserInput;
}
UserInput& NDS_getProcessingUserInput()
{
	assert(validToProcessInput);
	return intermediateUserInput;
}
bool NDS_isProcessingUserInput()
{
	return validToProcessInput;
}
const UserInput& NDS_getFinalUserInput()
{
	return finalUserInput;
}


static void saveUserInput(EMUFILE &os, UserInput &input)
{
	os.fwrite(input.buttons.array, 14);
	os.write_bool32(input.touch.isTouch);
	os.write_16LE(input.touch.touchX);
	os.write_16LE(input.touch.touchY);
	os.write_32LE(input.mic.micButtonPressed);
}
static bool loadUserInput(EMUFILE &is, UserInput &input, int version)
{
	is.fread(input.buttons.array, 14);
	is.read_bool32(input.touch.isTouch);
	is.read_16LE(input.touch.touchX);
	is.read_16LE(input.touch.touchY);
	is.read_32LE(input.mic.micButtonPressed);
	return true;
}
static void resetUserInput(UserInput &input)
{
	memset(&input, 0, sizeof(UserInput));
}
// (userinput is kind of a misnomer, e.g. finalUserInput has to mirror nds.pad, nds.touchX, etc.)
static void saveUserInput(EMUFILE &os)
{
	saveUserInput(os, finalUserInput);
	saveUserInput(os, intermediateUserInput); // saved in case a savestate is made during input processing (which Lua could do if nothing else)
	os.write_bool32(validToProcessInput);
	for (int i = 0; i < 14; i++)
		os.write_32LE(TurboTime.array[i]); // saved to make autofire more tolerable to use with re-recording
}
static bool loadUserInput(EMUFILE &is, int version)
{
	bool rv = true;
	rv &= loadUserInput(is, finalUserInput, version);
	rv &= loadUserInput(is, intermediateUserInput, version);
	is.read_bool32(validToProcessInput);
	for (int i = 0; i < 14; i++)
		is.read_32LE(TurboTime.array[i]);
	
	return rv;
}
static void resetUserInput()
{
	resetUserInput(finalUserInput);
	resetUserInput(intermediateUserInput);
}

static inline void gotInputRequest()
{
	// nobody should set the raw input while we're processing the input.
	// it might not screw anything up but it would be completely useless.
	assert(!validToProcessInput);
}

void NDS_setPad(bool R,bool L,bool D,bool U,bool T,bool S,bool B,bool A,bool Y,bool X,bool W,bool E,bool G, bool F)
{
	gotInputRequest();
	UserButtons& rawButtons = rawUserInput.buttons;
	rawButtons.R = R;
	rawButtons.L = L;
	rawButtons.D = D;
	rawButtons.U = U;
	rawButtons.T = T;
	rawButtons.S = S;
	rawButtons.B = B;
	rawButtons.A = A;
	rawButtons.Y = Y;
	rawButtons.X = X;
	rawButtons.W = W;
	rawButtons.E = E;
	rawButtons.G = G;
	rawButtons.F = F;
}
void NDS_setTouchPos(u16 x, u16 y)
{
	gotInputRequest();
	rawUserInput.touch.touchX = x<<4;
	rawUserInput.touch.touchY = y<<4;
	rawUserInput.touch.isTouch = true;

	if(movieMode != MOVIEMODE_INACTIVE && movieMode != MOVIEMODE_FINISHED)
	{
		// just in case, since the movie only stores 8 bits per touch coord
#ifdef MSB_FIRST
		rawUserInput.touch.touchX &= 0xF00F;
		rawUserInput.touch.touchY &= 0xF00F;
#else
		rawUserInput.touch.touchX &= 0x0FF0;
		rawUserInput.touch.touchY &= 0x0FF0;
#endif
	}
}
void NDS_releaseTouch(void)
{ 
	gotInputRequest();
	rawUserInput.touch.touchX = 0;
	rawUserInput.touch.touchY = 0;
	rawUserInput.touch.isTouch = false;
}
void NDS_setMic(bool pressed)
{
	gotInputRequest();
	rawUserInput.mic.micButtonPressed = (pressed ? TRUE : FALSE);
}


static void NDS_applyFinalInput();


void NDS_beginProcessingInput()
{
	// start off from the raw input
	intermediateUserInput = rawUserInput;

	// processing is valid now
	validToProcessInput = true;
}

void NDS_endProcessingInput()
{
	// transfer the processed input
	finalUserInput = intermediateUserInput;

	// processing is invalid now
	validToProcessInput = false;

	// use the final input for a few things right away
	NDS_applyFinalInput();
}








static void NDS_applyFinalInput()
{
	const UserInput& input = NDS_getFinalUserInput();

	u16	gbaKeys = ((input.buttons.A) ? 0 : (1 << 0)) |
	              ((input.buttons.B) ? 0 : (1 << 1)) |
	              ((input.buttons.T) ? 0 : (1 << 2)) |
	              ((input.buttons.S) ? 0 : (1 << 3)) |
	              ((input.buttons.R) ? 0 : (1 << 4)) |
	              ((input.buttons.L) ? 0 : (1 << 5)) |
	              ((input.buttons.U) ? 0 : (1 << 6)) |
	              ((input.buttons.D) ? 0 : (1 << 7)) |
	              ((input.buttons.E) ? 0 : (1 << 8)) |
	              ((input.buttons.W) ? 0 : (1 << 9));
	
	gbaKeys = LOCAL_TO_LE_16(gbaKeys);
	HostWriteWord(MMU.ARM9_REG, 0x130, gbaKeys);
	HostWriteWord(MMU.ARM7_REG, 0x130, gbaKeys);

	const u16 k_cnt_9 = HostReadWord(MMU.ARM9_REG, 0x132);
	if (k_cnt_9 & (1 << 14))
	{
		//INFO("ARM9: KeyPad IRQ (pad 0x%04X, cnt 0x%04X (condition %s))\n", pad, k_cnt_9, k_cnt_9 & (1<<15) ? "AND" : "OR");
		const u16 k_cnt_selected = (k_cnt_9 & 0x3F);
		if (k_cnt_9 & (1 << 15))	// AND
		{
			if ((~gbaKeys & k_cnt_selected) == k_cnt_selected)
				NDS_makeIrq(ARMCPU_ARM9,IRQ_BIT_KEYPAD);
		}
		else				// OR
		{
			if (~gbaKeys & k_cnt_selected)
				NDS_makeIrq(ARMCPU_ARM9,IRQ_BIT_KEYPAD);
		}
	}

	const u16 k_cnt_7 = HostReadWord(MMU.ARM7_REG, 0x132);
	if ( k_cnt_7 & (1 << 14))
	{
		//INFO("ARM7: KeyPad IRQ (pad 0x%04X, cnt 0x%04X (condition %s))\n", pad, k_cnt_7, k_cnt_7 & (1<<15) ? "AND" : "OR");
		const u16 k_cnt_selected = (k_cnt_7 & 0x3F);
		if (k_cnt_7 & (1 << 15))	// AND
		{
			if ((~gbaKeys & k_cnt_selected) == k_cnt_selected)
				NDS_makeIrq(ARMCPU_ARM7,IRQ_BIT_KEYPAD);
		}
		else				// OR
		{
			if (~gbaKeys & k_cnt_selected)
				NDS_makeIrq(ARMCPU_ARM7,IRQ_BIT_KEYPAD);
		}
	}
	
	if (input.touch.isTouch)
	{
		u16 adc_x = NDS_getADCTouchPosX(input.touch.touchX);
		u16 adc_y = NDS_getADCTouchPosY(input.touch.touchY);
		nds.adc_touchX = adc_x;
		nds.adc_touchY = adc_y;
		nds.adc_jitterctr = 0;

		nds.scr_touchX = input.touch.touchX;
		nds.scr_touchY = input.touch.touchY;
		nds.isTouch = 1;
	}
	else
	{
		nds.adc_touchX = 0;
		nds.adc_touchY = 0;
		nds.scr_touchX = 0;
		nds.scr_touchY = 0;
		nds.isTouch = 0;
	}

	if (input.buttons.F && !countLid) 
	{
		LidClosed = (!LidClosed) & 0x01;
		if (!LidClosed)
		{
			NDS_makeIrq(ARMCPU_ARM7,IRQ_BIT_ARM7_FOLD);
		}

		countLid = 30;
	}
	else 
	{
		if (countLid > 0)
			countLid--;
	}
	
	u16	ndsKeysExt = ((input.buttons.X) ? 0 : (1 << 0)) |
	                 ((input.buttons.Y) ? 0 : (1 << 1)) |
	                                          (1 << 2)  |
	                 ((input.buttons.G) ? 0 : (1 << 3)) | // debug button
	                                          (1 << 4)  |
	                                          (1 << 5)  |
	                 ((nds.isTouch)     ? 0 : (1 << 6)) |
	                 ((!LidClosed)      ? 0 : (1 << 7));
	
	T1WriteWord(MMU.ARM7_REG, 0x136, ndsKeysExt);
	
	InputDisplayString = MakeInputDisplayString(ndsKeysExt, gbaKeys);
}


void NDS_suspendProcessingInput(bool suspend)
{
	static int suspendCount = 0;
	if(suspend)
	{
		// enter non-processing block
		assert(validToProcessInput);
		validToProcessInput = false;
		suspendCount++;
	}
	else if(suspendCount)
	{
		// exit non-processing block
		validToProcessInput = true;
		suspendCount--;
	}
	else
	{
		// unwound past first time -> not processing
		validToProcessInput = false;
	}
}

void NDS_swapScreen()
{
	if (GPU->GetDisplayMain()->GetEngineID() == GPUEngineID_Main)
	{
		GPU->GetDisplayMain()->SetEngineByID(GPUEngineID_Sub);
		GPU->GetDisplayTouch()->SetEngineByID(GPUEngineID_Main);
	}
	else
	{
		GPU->GetDisplayMain()->SetEngineByID(GPUEngineID_Main);
		GPU->GetDisplayTouch()->SetEngineByID(GPUEngineID_Sub);
	}
}

NDSError NDS_GetLastError()
{
	return _lastNDSError;
}

void emu_halt(EmuHaltReasonCode reasonCode, NDSErrorTag errorTag)
{
	switch (reasonCode)
	{
		case EMUHALT_REASON_USER_REQUESTED_HALT:
			_lastNDSError.code = NDSError_NoError;
			_lastNDSError.tag = NDSErrorTag_None;
			break;
			
		case EMUHALT_REASON_SYSTEM_POWERED_OFF:
			_lastNDSError.code = NDSError_SystemPoweredOff;
			_lastNDSError.tag = NDSErrorTag_None;
			break;
			
		case EMUHALT_REASON_JIT_UNMAPPED_ADDRESS_EXCEPTION:
			_lastNDSError.code = NDSError_JITUnmappedAddressException;
			_lastNDSError.tag = errorTag;
			break;
			
		case EMUHALT_REASON_ARM_RESERVED_0X14_EXCEPTION:
		case EMUHALT_REASON_ARM_UNDEFINED_INSTRUCTION_EXCEPTION:
			_lastNDSError.code = NDSError_ARMUndefinedInstructionException;
			_lastNDSError.tag = errorTag;
			break;
			
		case EMUHALT_REASON_UNKNOWN:
		default:
			_lastNDSError.code = NDSError_UnknownError;
			_lastNDSError.tag = errorTag;
			break;
	}
	
	NDS_CurrentCPUInfoToNDSError(_lastNDSError);
	
	wifiHandler->CommStop();
	GPU->ForceFrameStop();
	execute = false;
	
	//printf("halting emu: ARM9 PC=%08X/%08X, ARM7 PC=%08X/%08X\n", NDS_ARM9.R[15], NDS_ARM9.instruct_adr, NDS_ARM7.R[15], NDS_ARM7.instruct_adr);
	
#ifdef LOG_ARM9
	if (fp_dis9)
	{
		char buf[256] = { 0 };
		sprintf(buf, "halting emu: ARM9 PC=%08X/%08X\n", NDS_ARM9.R[15], NDS_ARM9.instruct_adr);
		fwrite(buf, 1, strlen(buf), fp_dis9);
		INFO("ARM9 halted\n");
	}
#endif

#ifdef LOG_ARM7
	if (fp_dis7)
	{
		char buf[256] = { 0 };
		sprintf(buf, "halting emu: ARM7 PC=%08X/%08X\n", NDS_ARM7.R[15], NDS_ARM7.instruct_adr);
		fwrite(buf, 1, strlen(buf), fp_dis7);
		INFO("ARM7 halted\n");
	}
#endif
}

//returns true if exmemcnt specifies satisfactory parameters for the device, which calls this function
bool ValidateSlot2Access(u32 procnum, u32 demandSRAMSpeed, u32 demand1stROMSpeed, u32 demand2ndROMSpeed, int clockbits)
{
	static const u32 _sramSpeeds[] = {10,8,6,18};
	static const u32 _rom1Speeds[] = {10,8,6,18};
	static const u32 _rom2Speeds[] = {6,4};
	u16 exmemcnt = T1ReadWord(MMU.MMU_MEM[procnum][0x40], 0x204);
	u16 exmemcnt9 = T1ReadWord(MMU.ARM9_REG, 0x204);
	u32 arm7access = (exmemcnt9 & EXMEMCNT_MASK_SLOT2_ARM7);
	u32 sramSpeed = _sramSpeeds[(exmemcnt & EXMEMCNT_MASK_SLOT2_SRAM_TIME)];
	u32 romSpeed1 = _rom1Speeds[(exmemcnt & EXMEMCNT_MASK_SLOT2_ROM_1ST_TIME)>>2];
	u32 romSpeed2 = _rom2Speeds[(exmemcnt & EXMEMCNT_MASK_SLOT2_ROM_2ND_TIME)>>4];
	u32 curclockbits = (exmemcnt & EXMEMCNT_MASK_SLOT2_CLOCKRATE)>>5;
	
	if(procnum==ARMCPU_ARM9 && arm7access) return false;
	if(procnum==ARMCPU_ARM7 && !arm7access) return false;

	//what we're interested in here is whether the rom/ram are too low -> too fast. then accesses won't have enough time to work.
	//i'm not sure if this gives us enough flexibility, but it is good enough for now.
	//should make the arguments to this function bitmasks later if we need better.
	if(sramSpeed < demandSRAMSpeed) return false;
	if(romSpeed1 < demand1stROMSpeed) return false;
	if(romSpeed2 < demand2ndROMSpeed) return false;

	if(clockbits != -1 && clockbits != (int)curclockbits) return false;

	return true;
}

void NDS_GetCPULoadAverage(u32 &outLoadAvgARM9, u32 &outLoadAvgARM7)
{
	//calculate a 16 frame arm9 load average
	u32 calcLoad = 0;
	for (size_t i = 0; i < 16; i++)
	{
		//blend together a few frames to keep low-framerate games from having a jittering load average
		//(they will tend to work 100% for a frame and then sleep for a while)
		//4 frames should handle even the slowest of games
		u32 sample	= nds.runCycleCollector[ARMCPU_ARM9][(i + 0 + nds.idleFrameCounter) & 15]
					+ nds.runCycleCollector[ARMCPU_ARM9][(i + 1 + nds.idleFrameCounter) & 15]
					+ nds.runCycleCollector[ARMCPU_ARM9][(i + 2 + nds.idleFrameCounter) & 15]
					+ nds.runCycleCollector[ARMCPU_ARM9][(i + 3 + nds.idleFrameCounter) & 15];
		sample /= 4;
		calcLoad = calcLoad/8 + sample*7/8;
	}
	outLoadAvgARM9 = std::min<u32>( 100, std::max<u32>(0, (u32)(calcLoad*100/1120380)) );
	
	//calculate a 16 frame arm7 load average
	calcLoad = 0;
	for (size_t i = 0; i < 16; i++)
	{
		u32 sample	= nds.runCycleCollector[ARMCPU_ARM7][(i + 0 + nds.idleFrameCounter) & 15]
					+ nds.runCycleCollector[ARMCPU_ARM7][(i + 1 + nds.idleFrameCounter) & 15]
					+ nds.runCycleCollector[ARMCPU_ARM7][(i + 2 + nds.idleFrameCounter) & 15]
					+ nds.runCycleCollector[ARMCPU_ARM7][(i + 3 + nds.idleFrameCounter) & 15];
		sample /= 4;
		calcLoad = calcLoad/8 + sample*7/8;
	}
	outLoadAvgARM7 = std::min<u32>( 100, std::max<u32>(0, (u32)(calcLoad*100/1120380)) );
}

//these templates needed to be instantiated manually
template void NDS_exec<FALSE>(s32 nb);
template void NDS_exec<TRUE>(s32 nb);

void TCommonSettings::GameHacks::apply()
{
	clear();
	if(!en) return;

	flags.overclock = gameInfo.IsCode("IPK") || gameInfo.IsCode("IPG"); //HG/SS
	flags.stylusjitter = gameInfo.IsCode("YDM"); //CSI: Dark Motives
}

void TCommonSettings::GameHacks::clear()
{
	memset(&flags,0,sizeof(flags));
}
