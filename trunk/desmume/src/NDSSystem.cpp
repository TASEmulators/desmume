/*	Copyright (C) 2006 yopyop
	Copyright (C) 2008-2010 DeSmuME team

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

#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <math.h>
#include <zlib.h>

#include "common.h"
#include "NDSSystem.h"
#include "render3D.h"
#include "MMU.h"
#include "ROMReader.h"
#include "gfx3d.h"
#include "utils/decrypt/decrypt.h"
#include "utils/decrypt/crc.h"
#include "bios.h"
#include "debug.h"
#include "cheatSystem.h"
#include "movie.h"
#include "Disassembler.h"
#include "readwrite.h"
#include "debug.h"
#include "firmware.h"

#include "path.h"

PathInfo path;

TCommonSettings CommonSettings;
static BaseDriver _stub_driver;
BaseDriver* driver = &_stub_driver;
std::string InputDisplayString;

static BOOL LidClosed = FALSE;
static u8	countLid = 0;

GameInfo gameInfo;
NDSSystem nds;
CFIRMWARE	*firmware = NULL;

using std::min;
using std::max;

bool singleStep;
bool nds_debug_continuing[2];
int lagframecounter;
int LagFrameFlag;
int lastLag;
int TotalLagFrames;

TSCalInfo TSCal;

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

#ifdef GDB_STUB
int NDS_Init( struct armcpu_memory_iface *arm9_mem_if,
struct armcpu_ctrl_iface **arm9_ctrl_iface,
struct armcpu_memory_iface *arm7_mem_if,
struct armcpu_ctrl_iface **arm7_ctrl_iface) {
#else
int NDS_Init( void) {
#endif
	nds.idleFrameCounter = 0;
	memset(nds.runCycleCollector,0,sizeof(nds.runCycleCollector));
	MMU_Init();
	nds.VCount = 0;

	if (Screen_Init(GFXCORE_DUMMY) != 0)
		return -1;

	gfx3d_init();

#ifdef GDB_STUB
	armcpu_new(&NDS_ARM7,1, arm7_mem_if, arm7_ctrl_iface);
	armcpu_new(&NDS_ARM9,0, arm9_mem_if, arm9_ctrl_iface);
#else
	armcpu_new(&NDS_ARM7,1);
	armcpu_new(&NDS_ARM9,0);
#endif

	if (SPU_Init(SNDCORE_DUMMY, 740) != 0)
		return -1;

	WIFI_Init() ;

	// Init calibration info
	TSCal.adc.x1 = 0x0200;
	TSCal.adc.y1 = 0x0200;
	TSCal.scr.x1 = 0x20 + 1; // calibration screen coords are 1-based,
	TSCal.scr.y1 = 0x20 + 1; // either that or NDS_getADCTouchPosX/Y are wrong.
	TSCal.adc.x2 = 0x0E00;
	TSCal.adc.y2 = 0x0800;
	TSCal.scr.x2 = 0xE0 + 1;
	TSCal.scr.y2 = 0x80 + 1;
	TSCal.adc.width = (TSCal.adc.x2 - TSCal.adc.x1);
	TSCal.adc.height = (TSCal.adc.y2 - TSCal.adc.y1);
	TSCal.scr.width = (TSCal.scr.x2 - TSCal.scr.x1);
	TSCal.scr.height = (TSCal.scr.y2 - TSCal.scr.y1);

	cheats = new CHEATS();
	cheatSearch = new CHEATSEARCH();

	return 0;
}

void NDS_DeInit(void) {
	if(MMU.CART_ROM != MMU.UNUSED_RAM)
		NDS_FreeROM();

	SPU_DeInit();
	Screen_DeInit();
	MMU_DeInit();
	gpu3D->NDS_3D_Close();

	WIFI_DeInit();
	if (cheats)
		delete cheats;
	if (cheatSearch)
		delete cheatSearch;

}

BOOL NDS_SetROM(u8 * rom, u32 mask)
{
	MMU_setRom(rom, mask);

	return TRUE;
} 

NDS_header * NDS_getROMHeader(void)
{
	if(MMU.CART_ROM == MMU.UNUSED_RAM) return NULL;
	NDS_header * header = new NDS_header;

	memcpy(header->gameTile, MMU.CART_ROM, 12);
	memcpy(header->gameCode, MMU.CART_ROM + 12, 4);
	header->makerCode = T1ReadWord(MMU.CART_ROM, 16);
	header->unitCode = MMU.CART_ROM[18];
	header->deviceCode = MMU.CART_ROM[19];
	header->cardSize = MMU.CART_ROM[20];
	memcpy(header->cardInfo, MMU.CART_ROM + 21, 8);
	header->flags = MMU.CART_ROM[29];
	header->ARM9src = T1ReadLong(MMU.CART_ROM, 32);
	header->ARM9exe = T1ReadLong(MMU.CART_ROM, 36);
	header->ARM9cpy = T1ReadLong(MMU.CART_ROM, 40);
	header->ARM9binSize = T1ReadLong(MMU.CART_ROM, 44);
	header->ARM7src = T1ReadLong(MMU.CART_ROM, 48);
	header->ARM7exe = T1ReadLong(MMU.CART_ROM, 52);
	header->ARM7cpy = T1ReadLong(MMU.CART_ROM, 56);
	header->ARM7binSize = T1ReadLong(MMU.CART_ROM, 60);
	header->FNameTblOff = T1ReadLong(MMU.CART_ROM, 64);
	header->FNameTblSize = T1ReadLong(MMU.CART_ROM, 68);
	header->FATOff = T1ReadLong(MMU.CART_ROM, 72);
	header->FATSize = T1ReadLong(MMU.CART_ROM, 76);
	header->ARM9OverlayOff = T1ReadLong(MMU.CART_ROM, 80);
	header->ARM9OverlaySize = T1ReadLong(MMU.CART_ROM, 84);
	header->ARM7OverlayOff = T1ReadLong(MMU.CART_ROM, 88);
	header->ARM7OverlaySize = T1ReadLong(MMU.CART_ROM, 92);
	header->unknown2a = T1ReadLong(MMU.CART_ROM, 96);
	header->unknown2b = T1ReadLong(MMU.CART_ROM, 100);
	header->IconOff = T1ReadLong(MMU.CART_ROM, 104);
	header->CRC16 = T1ReadWord(MMU.CART_ROM, 108);
	header->ROMtimeout = T1ReadWord(MMU.CART_ROM, 110);
	header->ARM9unk = T1ReadLong(MMU.CART_ROM, 112);
	header->ARM7unk = T1ReadLong(MMU.CART_ROM, 116);
	memcpy(header->unknown3c, MMU.CART_ROM + 120, 8);
	header->ROMSize = T1ReadLong(MMU.CART_ROM, 128);
	header->HeaderSize = T1ReadLong(MMU.CART_ROM, 132);
	memcpy(header->unknown5, MMU.CART_ROM + 136, 56);
	memcpy(header->logo, MMU.CART_ROM + 192, 156);
	header->logoCRC16 = T1ReadWord(MMU.CART_ROM, 348);
	header->headerCRC16 = T1ReadWord(MMU.CART_ROM, 350);
	memcpy(header->reserved, MMU.CART_ROM + 352, min(160, (int)gameInfo.romsize - 352));

	return header;
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

#define DSGBA_LOADER_SIZE 512
enum
{
	ROM_NDS = 0,
	ROM_DSGBA
};

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

void GameInfo::populate()
{
	const char *regions[] = {	"JPFSEDIRKH",
								"JPN",
								"EUR",
								"FRA",
								"ESP",
								"USA",
								"NOE",
								"ITA",
								"RUS",
								"KOR",
								"HOL",

	};

	NDS_header * _header = NDS_getROMHeader();
	header = *_header;
	delete _header;

	memset(ROMserial, 0, sizeof(ROMserial));
	memset(ROMname, 0, sizeof(ROMname));
	memset(ROMfullName, 0, sizeof(ROMfullName));

	if (
		// ??? in all Homebrews game title have is 2E0000EA
		//(
		//(header->gameTile[0] == 0x2E) && 
		//(header->gameTile[1] == 0x00) && 
		//(header->gameTile[2] == 0x00) && 
		//(header->gameTile[3] == 0xEA)
		//) &&
		(
			((header.gameCode[0] == 0x23) && 
			(header.gameCode[1] == 0x23) && 
			(header.gameCode[2] == 0x23) && 
			(header.gameCode[3] == 0x23)
			) || 
			(header.gameCode[0] == 0x00) 
		)
		&&
			header.makerCode == 0x0
		)
	{
		strcpy(ROMserial, "Homebrew");
	}
	else
	{
		strcpy(ROMserial,"NTR-    -");
		memcpy(ROMserial+4, header.gameCode, 4);

		u32 region = (u32)(std::max<s32>(strchr(regions[0],header.gameCode[3]) - regions[0] + 1, 0));
		if (region != 0)
			strcat(ROMserial, regions[region]);
		else
			strcat(ROMserial, "Unknown");
		memset(ROMname, 0, sizeof(ROMname));
		memcpy(ROMname, header.gameTile, 12);
		trim(ROMname);

		u8 num = (T1ReadByte((u8*)romdata, header.IconOff) == 1)?6:7;
		for (int i = 0; i < num; i++)
		{
			memset(ROMfullName[i],0,sizeof(ROMfullName[i]));
			wcstombs(ROMfullName[i], (wchar_t *)(romdata+header.IconOff+0x240+(i*0x100)), 0x100);
			trim(ROMfullName[i]);
		}

	}
}
#ifdef WIN32

static std::vector<char> buffer;
static std::vector<char> v;

static void loadrom(std::string fname) {

	FILE* inf = fopen(fname.c_str(),"rb");
	if(!inf) return;

	fseek(inf,0,SEEK_END);
	int size = ftell(inf);
	fseek(inf,0,SEEK_SET);

	gameInfo.resize(size);
	fread(gameInfo.romdata,1,size,inf);
	gameInfo.fillGap();
	
	fclose(inf);
}

int NDS_LoadROM(const char *filename, const char *logicalFilename)
{
	int	type = ROM_NDS;
	char buf[MAX_PATH];

	if (filename == NULL)
		return -1;
	
    path.init(logicalFilename);

	if ( path.isdsgba(path.path)) {
		type = ROM_DSGBA;
		loadrom(path.path);
	}
	else if ( !strcasecmp(path.extension().c_str(), "nds")) {
		type = ROM_NDS;
		loadrom(path.path); //n.b. this does nothing if the file can't be found (i.e. if it was an extracted tempfile)...
		//...but since the data was extracted to gameInfo then it is ok
	}
	//ds.gba in archives, it's already been loaded into memory at this point
	else if (path.isdsgba(std::string(logicalFilename))) {
		type = ROM_DSGBA;
	} else {
		//well, try to load it as an nds rom anyway
		type = ROM_NDS;
		loadrom(path.path);
	}

	if(type == ROM_DSGBA)
	{
		std::vector<char> v(gameInfo.romdata + DSGBA_LOADER_SIZE, gameInfo.romdata + gameInfo.romsize);
		gameInfo.loadData(&v[0],gameInfo.romsize - DSGBA_LOADER_SIZE);
	}

	//check that size is at least the size of the header
	if (gameInfo.romsize < 352) {
		return -1;
	}
	


	//decrypt if necessary..
	//but this is untested and suspected to fail on big endian, so lets not support this on big endian

#ifndef WORDS_BIGENDIAN
	bool okRom = DecryptSecureArea((u8*)gameInfo.romdata,gameInfo.romsize);

	if(!okRom) {
		printf("Specified file is not a valid rom\n");
		return -1;
	}
#endif

	if (cheatSearch)
		cheatSearch->close();
	FCEUI_StopMovie();

	MMU_unsetRom();
	NDS_SetROM((u8*)gameInfo.romdata, gameInfo.mask);
	NDS_Reset();

	memset(buf, 0, MAX_PATH);

	path.getpathnoext(path.BATTERY, buf);
	
	strcat(buf, ".dsv");							// DeSmuME memory card	:)

	MMU_new.backupDevice.load_rom(buf);

	memset(buf, 0, MAX_PATH);

	path.getpathnoext(path.CHEATS, buf);
	
	strcat(buf, ".dct");							// DeSmuME cheat		:)
	cheats->init(buf);

	gameInfo.populate();
	gameInfo.crc = crc32(0,(u8*)gameInfo.romdata,gameInfo.romsize);
	INFO("\nROM crc: %08X\n", gameInfo.crc);
	INFO("ROM serial: %s\n", gameInfo.ROMserial);
	INFO("ROM internal name: %s\n\n", gameInfo.ROMname);

	return 1;
}
#else
int NDS_LoadROM(const char *filename, const char *logicalFilename)
{
	int					ret;
	int					type;
	ROMReader_struct	*reader;
	void				*file;
	u32					size;
	u8					*data;
	char				*noext;
	char				buf[MAX_PATH];

	if (filename == NULL)
		return -1;

	noext = strdup(filename);
	reader = ROMReaderInit(&noext);
	
	if(logicalFilename) path.init(logicalFilename);
	else path.init(filename);
	if(!strcasecmp(path.extension().c_str(), "zip"))		type = ROM_NDS;
	else if ( !strcasecmp(path.extension().c_str(), "nds"))
		type = ROM_NDS;
	else if ( path.isdsgba(path.path))
		type = ROM_DSGBA;
	else
		type = ROM_NDS;

	file = reader->Init(filename);
	if (!file)
	{
		reader->DeInit(file);
		free(noext);
		return -1;
	}

	size = reader->Size(file);

	if(type == ROM_DSGBA)
	{
		reader->Seek(file, DSGBA_LOADER_SIZE, SEEK_SET);
		size -= DSGBA_LOADER_SIZE;
	}

	//check that size is at least the size of the header
	if (size < 352) {
		reader->DeInit(file);
		free(noext);
		return -1;
	}

	gameInfo.resize(size);

	// Make sure old ROM is freed first(at least this way we won't be eating
	// up a ton of ram before the old ROM is freed)
	if(MMU.CART_ROM != MMU.UNUSED_RAM)
		NDS_FreeROM();

	data = new u8[gameInfo.mask + 1];
	if (!data)
	{
		reader->DeInit(file);
		free(noext);
		return -1;
	}

	ret = reader->Read(file, data, size);
	reader->DeInit(file);

	//decrypt if necessary..
	//but this is untested and suspected to fail on big endian, so lets not support this on big endian
#ifndef WORDS_BIGENDIAN
	bool okRom = DecryptSecureArea(data,size);

	if(!okRom) {
		printf("Specified file is not a valid rom\n");
		return -1;
	}
#endif


	if (cheatSearch)
		cheatSearch->close();
	MMU_unsetRom();
	NDS_SetROM(data, gameInfo.mask);
	NDS_Reset();

	free(noext);

	memset(buf, 0, MAX_PATH);

	path.getpathnoext(path.BATTERY, buf);
	
	strcat(buf, ".dsv");							// DeSmuME memory card	:)

	MMU_new.backupDevice.load_rom(buf);

	memset(buf, 0, MAX_PATH);

	path.getpathnoext(path.CHEATS, buf);
	
	strcat(buf, ".dct");							// DeSmuME cheat		:)
	cheats->init(buf);

	gameInfo.populate();
	gameInfo.crc = crc32(0,data,size);
	INFO("\nROM crc: %08X\n", gameInfo.crc);
	INFO("ROM serial: %s\n", gameInfo.ROMserial);
	INFO("ROM internal name: %s\n\n", gameInfo.ROMname);

	return ret;
}
#endif
void NDS_FreeROM(void)
{
	FCEUI_StopMovie();
	if ((u8*)MMU.CART_ROM == (u8*)gameInfo.romdata)
		gameInfo.romdata = NULL;
	if (MMU.CART_ROM != MMU.UNUSED_RAM)
		delete [] MMU.CART_ROM;
	MMU_unsetRom();
}



int NDS_ImportSave(const char *filename)
{
	if (strlen(filename) < 4)
		return 0;

	if (memcmp(filename+strlen(filename)-4, ".duc", 4) == 0)
		return MMU_new.backupDevice.load_duc(filename);
	else
		if (MMU_new.backupDevice.load_no_gba(filename))
			return 1;
		else
			return MMU_new.backupDevice.load_raw(filename);

	return 0;
}

bool NDS_ExportSave(const char *filename)
{
	if (strlen(filename) < 4)
		return false;

	if (memcmp(filename+strlen(filename)-5, ".sav*", 5) == 0)
	{
		char tmp[MAX_PATH];
		memset(tmp, 0, MAX_PATH);
		strcpy(tmp, filename);
		tmp[strlen(tmp)-1] = 0;
		return MMU_new.backupDevice.save_no_gba(tmp);
	}

	if (memcmp(filename+strlen(filename)-4, ".sav", 4) == 0)
		return MMU_new.backupDevice.save_raw(filename);

	return false;
}

static int WritePNGChunk(FILE *fp, uint32 size, const char *type, const uint8 *data)
{
	uint32 crc;

	uint8 tempo[4];

	tempo[0]=size>>24;
	tempo[1]=size>>16;
	tempo[2]=size>>8;
	tempo[3]=size;

	if(fwrite(tempo,4,1,fp)!=1)
		return 0;
	if(fwrite(type,4,1,fp)!=1)
		return 0;

	if(size)
		if(fwrite(data,1,size,fp)!=size)
			return 0;

	crc = crc32(0,(uint8 *)type,4);
	if(size)
		crc = crc32(crc,data,size);

	tempo[0]=crc>>24;
	tempo[1]=crc>>16;
	tempo[2]=crc>>8;
	tempo[3]=crc;

	if(fwrite(tempo,4,1,fp)!=1)
		return 0;
	return 1;
}
int NDS_WritePNG(const char *fname)
{
	int x, y;
	int width=256;
	int height=192*2;
	u16 * bmp = (u16 *)GPU_screen;
	FILE *pp=NULL;
	uint8 *compmem = NULL;
	uLongf compmemsize = (uLongf)( (height * (width + 1) * 3 * 1.001 + 1) + 12 );

	if(!(compmem=(uint8 *)malloc(compmemsize)))
		return 0;

	if(!(pp=fopen(fname, "wb")))
	{
		goto PNGerr;
	}
	{
		const uint8 header[8]={137,80,78,71,13,10,26,10};
		if(fwrite(header,8,1,pp)!=1)
			goto PNGerr;
	}

	{
		uint8 chunko[13];

		chunko[0] = width >> 24;		// Width
		chunko[1] = width >> 16;
		chunko[2] = width >> 8;
		chunko[3] = width;

		chunko[4] = height >> 24;		// Height
		chunko[5] = height >> 16;
		chunko[6] = height >> 8;
		chunko[7] = height;

		chunko[8]=8;				// 8 bits per sample(24 bits per pixel)
		chunko[9]=2;				// Color type; RGB triplet
		chunko[10]=0;				// compression: deflate
		chunko[11]=0;				// Basic adapative filter set(though none are used).
		chunko[12]=0;				// No interlace.

		if(!WritePNGChunk(pp,13,"IHDR",chunko))
			goto PNGerr;
	}

	{
		uint8 *tmp_buffer;
		uint8 *tmp_inc;
		tmp_inc = tmp_buffer = (uint8 *)malloc((width * 3 + 1) * height);

		for(y=0;y<height;y++)
		{
			*tmp_inc = 0;
			tmp_inc++;
			for(x=0;x<width;x++)
			{
				int r,g,b;
				u16 pixel = bmp[y*256+x];
				r = pixel>>10;
				pixel-=r<<10;
				g = pixel>>5;
				pixel-=g<<5;
				b = pixel;
				r*=255/31;
				g*=255/31;
				b*=255/31;
				tmp_inc[0] = b;
				tmp_inc[1] = g;
				tmp_inc[2] = r;
				tmp_inc += 3;
			}
		}

		if(compress(compmem, &compmemsize, tmp_buffer, height * (width * 3 + 1))!=Z_OK)
		{
			if(tmp_buffer) free(tmp_buffer);
			goto PNGerr;
		}
		if(tmp_buffer) free(tmp_buffer);
		if(!WritePNGChunk(pp,compmemsize,"IDAT",compmem))
			goto PNGerr;
	}
	if(!WritePNGChunk(pp,0,"IEND",0))
		goto PNGerr;

	free(compmem);
	fclose(pp);

	return 1;

PNGerr:
	if(compmem)
		free(compmem);
	if(pp)
		fclose(pp);
	return(0);
}

typedef struct
{
	u32 size;
	s32 width;
	s32 height;
	u16 planes;
	u16 bpp;
	u32 cmptype;
	u32 imgsize;
	s32 hppm;
	s32 vppm;
	u32 numcol;
	u32 numimpcol;
} bmpimgheader_struct;

#include "PACKED.h"
typedef struct
{
	u16 id __PACKED;
	u32 size __PACKED;
	u16 reserved1 __PACKED;
	u16 reserved2 __PACKED;
	u32 imgoffset __PACKED;
} bmpfileheader_struct;
#include "PACKED_END.h"

int NDS_WriteBMP(const char *filename)
{
	bmpfileheader_struct fileheader;
	bmpimgheader_struct imageheader;
	FILE *file;
	int i,j;
	u16 * bmp = (u16 *)GPU_screen;
	size_t elems_written = 0;

	memset(&fileheader, 0, sizeof(fileheader));
	fileheader.size = sizeof(fileheader);
	fileheader.id = 'B' | ('M' << 8);
	fileheader.imgoffset = sizeof(fileheader)+sizeof(imageheader);

	memset(&imageheader, 0, sizeof(imageheader));
	imageheader.size = sizeof(imageheader);
	imageheader.width = 256;
	imageheader.height = 192*2;
	imageheader.planes = 1;
	imageheader.bpp = 24;
	imageheader.cmptype = 0; // None
	imageheader.imgsize = imageheader.width * imageheader.height * 3;

	if ((file = fopen(filename,"wb")) == NULL)
		return 0;

	elems_written += fwrite(&fileheader, 1, sizeof(fileheader), file);
	elems_written += fwrite(&imageheader, 1, sizeof(imageheader), file);

	for(j=0;j<192*2;j++)
	{
		for(i=0;i<256;i++)
		{
			u8 r,g,b;
			u16 pixel = bmp[(192*2-j-1)*256+i];
			r = pixel>>10;
			pixel-=r<<10;
			g = pixel>>5;
			pixel-=g<<5;
			b = (u8)pixel;
			r*=255/31;
			g*=255/31;
			b*=255/31;
			elems_written += fwrite(&r, 1, sizeof(u8), file); 
			elems_written += fwrite(&g, 1, sizeof(u8), file); 
			elems_written += fwrite(&b, 1, sizeof(u8), file);
		}
	}
	fclose(file);

	return 1;
}

int NDS_WriteBMP_32bppBuffer(int width, int height, const void* buf, const char *filename)
{
	bmpfileheader_struct fileheader;
	bmpimgheader_struct imageheader;
	FILE *file;
	size_t elems_written = 0;
	memset(&fileheader, 0, sizeof(fileheader));
	fileheader.size = sizeof(fileheader);
	fileheader.id = 'B' | ('M' << 8);
	fileheader.imgoffset = sizeof(fileheader)+sizeof(imageheader);

	memset(&imageheader, 0, sizeof(imageheader));
	imageheader.size = sizeof(imageheader);
	imageheader.width = width;
	imageheader.height = height;
	imageheader.planes = 1;
	imageheader.bpp = 32;
	imageheader.cmptype = 0; // None
	imageheader.imgsize = imageheader.width * imageheader.height * 4;

	if ((file = fopen(filename,"wb")) == NULL)
		return 0;

	elems_written += fwrite(&fileheader, 1, sizeof(fileheader), file);
	elems_written += fwrite(&imageheader, 1, sizeof(imageheader), file);

	for(int i=0;i<height;i++)
		for(int x=0;x<width;x++)
		{
			u8* pixel = (u8*)buf + (height-i-1)*width*4;
			pixel += (x*4);
			elems_written += fwrite(pixel+2,1,1,file);
			elems_written += fwrite(pixel+1,1,1,file);
			elems_written += fwrite(pixel+0,1,1,file);
			elems_written += fwrite(pixel+3,1,1,file);
		}
	fclose(file);

	return 1;
}

void NDS_Sleep() { nds.sleeping = TRUE; }
void NDS_ToggleCardEject()
{
	if(!nds.cardEjected)
	{
		//staff of kings will test this (it also uses the arm9 0xB8 poll)
		NDS_makeInt(1, 20);
	}
	nds.cardEjected ^= TRUE;
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
		bool capturing = (MainScreen.gpu->dispCapCnt.enabled || (MainScreen.gpu->dispCapCnt.val & 0x80000000));

		if(capturing && consecutiveNonCaptures > 30)
		{
			// the worst-looking graphics corruption problems from frameskip
			// are the result of skipping the capture on first frame it turns on.
			// so we do this to handle the capture immediately,
			// despite the risk of 1 frame of 2d/3d mismatch or wrong screen display.
			SkipNext2DFrame = false;
			nextSkip = false;
		}
		else if(lastOffset != MainScreen.offset && lastSkip && !skipped)
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
		lastLastOffset = lastOffset;
		lastOffset = MainScreen.offset;
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
		lastOffset = 0;
		SkipCur2DFrame = false;
		SkipCur3DFrame = false;
		SkipNext2DFrame = false;
		consecutiveNonCaptures = 0;
	}
private:
	bool nextSkip;
	bool skipped;
	bool lastSkip;
	int lastOffset;
	int lastLastOffset;
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
	ESI_DISPCNT_HStart, ESI_DISPCNT_HBlank
};

u64 nds_timer;
u64 nds_arm9_timer, nds_arm7_timer;

static const u64 kNever = 0xFFFFFFFFFFFFFFFFULL;

struct TSequenceItem
{
	u64 timestamp;
	u32 param;
	bool enabled;

	virtual void save(EMUFILE* os)
	{
		write64le(timestamp,os);
		write32le(param,os);
		writebool(enabled,os);
	}

	virtual bool load(EMUFILE* is)
	{
		if(read64le(&timestamp,is) != 1) return false;
		if(read32le(&param,is) != 1) return false;
		if(readbool(&enabled,is) != 1) return false;
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
		bool first = true, over;
		//we'll need to check chained timers..
		for(int i=num;i<4;i++)
		{
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
					if(procnum==0) NDS_makeARM9Int(3 + i);
					else NDS_makeARM7Int(3 + i);
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
		return (controller->check && nds_timer>= controller->nextEvent);
	}

	FORCEINLINE bool isEnabled() { 
		return controller->check?TRUE:FALSE;
	}

	FORCEINLINE u64 next()
	{
		return controller->nextEvent;
	}

	FORCEINLINE void exec()
	{
		IF_DEVELOPER(DEBUG_statistics.sequencerExecutionCounters[5+procnum*4+chan]++);

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
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A0, (u32)MMU.divResult);
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A4, (u32)(MMU.divResult >> 32));
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A8, (u32)MMU.divMod);
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2AC, (u32)(MMU.divMod >> 32));
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
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2B4, MMU.sqrtResult);
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

	void save(EMUFILE* os)
	{
		write64le(nds_timer,os);
		write64le(nds_arm9_timer,os);
		write64le(nds_arm7_timer,os);
		dispcnt.save(os);
		divider.save(os);
		sqrtunit.save(os);
		gxfifo.save(os);
		wifi.save(os);
#define SAVE(I,X,Y) I##_##X##_##Y .save(os);
		SAVE(timer,0,0); SAVE(timer,0,1); SAVE(timer,0,2); SAVE(timer,0,3); 
		SAVE(timer,1,0); SAVE(timer,1,1); SAVE(timer,1,2); SAVE(timer,1,3); 
		SAVE(dma,0,0); SAVE(dma,0,1); SAVE(dma,0,2); SAVE(dma,0,3); 
		SAVE(dma,1,0); SAVE(dma,1,1); SAVE(dma,1,2); SAVE(dma,1,3); 
#undef SAVE
	}

	bool load(EMUFILE* is, int version)
	{
		if(read64le(&nds_timer,is) != 1) return false;
		if(read64le(&nds_arm9_timer,is) != 1) return false;
		if(read64le(&nds_arm7_timer,is) != 1) return false;
		if(!dispcnt.load(is)) return false;
		if(!divider.load(is)) return false;
		if(!sqrtunit.load(is)) return false;
		if(!gxfifo.load(is)) return false;
		if(version >= 1) if(!wifi.load(is)) return false;
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
const u64 kWifiCycles = 34*2;
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


	#ifdef EXPERIMENTAL_WIFI_COMM
	wifi.enabled = true;
	wifi.timestamp = kWifiCycles;
	#else
	wifi.enabled = false;
	#endif
}

//this isnt helping much right now. work on it later
//#include "utils/task.h"
//Task taskSubGpu(true);
//void* renderSubScreen(void*)
//{
//	GPU_RenderLine(&SubScreen, nds.VCount, SkipCur2DFrame);
//	return NULL;
//}

static void execHardware_hblank()
{
	//turn on hblank status bit
	T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) | 2);
	T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 2);

	//fire hblank interrupts if necessary
	NDS_ARM9HBlankInt();
	NDS_ARM7HBlankInt();

	//emulation housekeeping. for some reason we always do this at hblank,
	//even though it sounds more reasonable to do it at hstart
	SPU_Emulate_core();
	driver->AVI_SoundUpdate(SPU_core->outbuf,spu_core_samples);
	WAV_WavSoundUpdate(SPU_core->outbuf,spu_core_samples);

	//this logic was formerly at hblank time. it was moved to the beginning of the scanline on a whim
	if(nds.VCount<192)
	{
		//so, we have chosen to do the line drawing at hblank time.
		//this is the traditional time for it in desmume.
		//while it may seem more ruthlessly accurate to do it at hstart,
		//in practice we need to be more forgiving, in case things have overrun the scanline start.
		//this should be safe since games cannot do anything timing dependent until this next
		//scanline begins, anyway (as this scanline was in the middle of drawing)
		//taskSubGpu.execute(renderSubScreen,NULL);
		GPU_RenderLine(&MainScreen, nds.VCount, frameSkipper.ShouldSkip2D());
		GPU_RenderLine(&SubScreen, nds.VCount, frameSkipper.ShouldSkip2D());
		//taskSubGpu.finish();

		//trigger hblank dmas
		//but notice, we do that just after we finished drawing the line
		//(values copied by this hdma should not be used until the next scanline)
		triggerDma(EDMAMode_HBlank);
	}
}

static void execHardware_hstart_vblankEnd()
{
	nds.VCount = 0;
	sequencer.nds_vblankEnded = true;
	sequencer.reschedule = true;

	//turn off vblank status bit
	T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) & 0xFFFE);
	T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFE);

	//some emulation housekeeping
	frameSkipper.Advance();
}

static void execHardware_hstart_vblankStart()
{
	//printf("--------VBLANK!!!--------\n");

	//turn on vblank status bit
	T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) | 1);
	T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 1);

	//fire vblank interrupts if necessary
	NDS_ARM9VBlankInt();
	NDS_ARM7VBlankInt();

	//some emulation housekeeping
	gfx3d_VBlankSignal();

	//trigger vblank dmas
	triggerDma(EDMAMode_VBlank);

	//tracking for arm9 load average
	nds.runCycleCollector[nds.idleFrameCounter] = 1120380-nds.idleCycles;
	nds.idleFrameCounter++;
	nds.idleFrameCounter &= 15;
	nds.idleCycles = 0;
}

static void execHardware_hstart_vcount()
{
	u16 vmatch = T1ReadWord(MMU.ARM9_REG, 4);
	vmatch = ((vmatch>>8)|((vmatch<<1)&(1<<8)));
	if(nds.VCount==vmatch)
	{
		//arm9 vmatch
		T1WriteWord(MMU.ARM9_REG, 4, T1ReadWord(MMU.ARM9_REG, 4) | 4);
		if(T1ReadWord(MMU.ARM9_REG, 4) & 32) {
			NDS_makeARM9Int(2);
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
			NDS_makeARM7Int(2);
	}
	else
		T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFB);
}

static void execHardware_hstart()
{
	nds.VCount++;

	if(nds.VCount==263)
	{
		execHardware_hstart_vblankEnd();
	} else if(nds.VCount==192)
	{
		execHardware_hstart_vblankStart();
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

	if(nds.VCount<192)
	{
		//this is hacky.
		//there is a corresponding hack in doDMA.
		//it should be driven by a fifo (and generate just in time as the scanline is displayed)
		//but that isnt even possible until we have some sort of sub-scanline timing.
		//it may not be necessary.
		triggerDma(EDMAMode_MemDisplay);
	}

	//end of 3d vblank
	if(nds.VCount==214)
	{
		gfx3d_VBlankEndSignal(frameSkipper.ShouldSkip3D());
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

#ifdef EXPERIMENTAL_WIFI_COMM
	next = _fast_min(next,wifi.next());
#endif

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
			dispcnt.timestamp += 3168;
			dispcnt.param = ESI_DISPCNT_HBlank;
			break;
		case ESI_DISPCNT_HBlank:
			execHardware_hblank();
			dispcnt.timestamp += 1092;
			dispcnt.param = ESI_DISPCNT_HStart;
			break;
		}
	}

#ifdef EXPERIMENTAL_WIFI_COMM
	if(wifi.isTriggered())
	{
		WIFI_usTrigger();
		wifi.timestamp += kWifiCycles;
	}
#endif
	
	if(divider.isTriggered()) divider.exec();
	if(sqrtunit.isTriggered()) sqrtunit.exec();
	if(gxfifo.isTriggered()) gxfifo.exec();


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

static void saveUserInput(EMUFILE* os);
static bool loadUserInput(EMUFILE* is, int version);

void nds_savestate(EMUFILE* os)
{
	//version
	write32le(3,os);

	sequencer.save(os);

	saveUserInput(os);

	write32le(LidClosed,os);
	write8le(countLid,os);
}

bool nds_loadstate(EMUFILE* is, int size)
{
	// this isn't part of the savestate loading logic, but
	// don't skip the next frame after loading a savestate
	frameSkipper.OmitSkip(true, true);

	//read version
	u32 version;
	if(read32le(&version,is) != 1) return false;

	if(version > 3) return false;

	bool temp = true;
	temp &= sequencer.load(is, version);
	if(version <= 1 || !temp) return temp;
	temp &= loadUserInput(is, version);

	if(version < 3) return temp;

	read32le(&LidClosed,is);
	read8le(&countLid,is);

	return temp;
}

//#define LOG_ARM9
//#define LOG_ARM7
//bool dolog = false;

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

		printf("%05d:%03d %12lld 9:%08X %08X %-30s R00:%08X R01:%08X R02:%08X R03:%08X R04:%08X R05:%08X R06:%08X R07:%08X R08:%08X R09:%08X R10:%08X R11:%08X R12:%08X R13:%08X R14:%08X R15:%08X\n",
			currFrameCounter, nds.VCount, nds_timer, 
			NDS_ARM9.instruct_adr,NDS_ARM9.instruction, dasmbuf, 
			NDS_ARM9.R[0],  NDS_ARM9.R[1],  NDS_ARM9.R[2],  NDS_ARM9.R[3],  NDS_ARM9.R[4],  NDS_ARM9.R[5],  NDS_ARM9.R[6],  NDS_ARM9.R[7], 
			NDS_ARM9.R[8],  NDS_ARM9.R[9],  NDS_ARM9.R[10],  NDS_ARM9.R[11],  NDS_ARM9.R[12],  NDS_ARM9.R[13],  NDS_ARM9.R[14],  NDS_ARM9.R[15]);  
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
		
		printf("%05d:%03d %12lld 7:%08X %08X %-30s R00:%08X R01:%08X R02:%08X R03:%08X R04:%08X R05:%08X R06:%08X R07:%08X R08:%08X R09:%08X R10:%08X R11:%08X R12:%08X R13:%08X R14:%08X R15:%08X\n",
			currFrameCounter, nds.VCount, nds_timer, 
			NDS_ARM7.instruct_adr,NDS_ARM7.instruction, dasmbuf, 
			NDS_ARM7.R[0],  NDS_ARM7.R[1],  NDS_ARM7.R[2],  NDS_ARM7.R[3],  NDS_ARM7.R[4],  NDS_ARM7.R[5],  NDS_ARM7.R[6],  NDS_ARM7.R[7], 
			NDS_ARM7.R[8],  NDS_ARM7.R[9],  NDS_ARM7.R[10],  NDS_ARM7.R[11],  NDS_ARM7.R[12],  NDS_ARM7.R[13],  NDS_ARM7.R[14],  NDS_ARM7.R[15]);
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

template<bool doarm9, bool doarm7>
static /*donotinline*/ std::pair<s32,s32> armInnerLoop(
	const u64 nds_timer_base, const s32 s32next, s32 arm9, s32 arm7)
{
	s32 timer = minarmtime<doarm9,doarm7>(arm9,arm7);
	while(timer < s32next && !sequencer.reschedule)
	{
		if(doarm9 && (!doarm7 || arm9 <= timer))
		{
			if(!NDS_ARM9.waitIRQ)
			{
				arm9log();
				arm9 += armcpu_exec<ARMCPU_ARM9>();
				#ifdef DEVELOPER
					nds_debug_continuing[0] = false;
				#endif
			}
			else
			{
				s32 temp = arm9;
				arm9 = min(s32next, arm9 + kIrqWait);
				nds.idleCycles += arm9-temp;
			}
		}
		if(doarm7 && (!doarm9 || arm7 <= timer))
		{
			if(!NDS_ARM7.waitIRQ)
			{
				arm7log();
				arm7 += (armcpu_exec<ARMCPU_ARM7>()<<1);
				#ifdef DEVELOPER
					nds_debug_continuing[1] = false;
				#endif
			}
			else
			{
				arm7 = min(s32next, arm7 + kIrqWait);
				if(arm7 == s32next)
				{
					nds_timer = nds_timer_base + minarmtime<doarm9,false>(arm9,arm7);
					return armInnerLoop<doarm9,false>(nds_timer_base, s32next, arm9, arm7);
				}
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
}

void NDS_debug_step()
{
	NDS_debug_continue();
	singleStep = true;
}

template<bool FORCE>
void NDS_exec(s32 nb)
{
	LagFrameFlag=1;

	if((currFrameCounter&63) == 0)
		MMU_new.backupDevice.lazy_flush();

	sequencer.nds_vblankEnded = false;

	nds.cpuloopIterationCount = 0;

	IF_DEVELOPER(for(int i=0;i<32;i++) DEBUG_statistics.sequencerExecutionCounters[i] = 0);

	if(nds.sleeping)
	{
		gpu_UpdateRender();

		//speculative code: if ANY irq happens, wake up the arm7.
		//I think the arm7 program analyzes the system and may decide not to wake up
		//if it is dissatisfied with the conditions
		if((MMU.reg_IE[1] & MMU.reg_IF[1]))
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
				//(gbd stub doesnt yet know how to trigger these immediately by calling reschedule)
				while((NDS_ARM9.stalled || NDS_ARM7.stalled) && execute)
				{
					driver->EMU_DebugIdleUpdate();
					nds_debug_continuing[0] = nds_debug_continuing[1] = true;
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

			std::pair<s32,s32> arm9arm7 = armInnerLoop<true,true>(nds_timer_base,s32next,arm9,arm7);

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
			//let's re-analyze it after this hardware event
			if(NDS_ARM9.waitIRQ) nds_arm9_timer = nds_timer;
			if(NDS_ARM7.waitIRQ) nds_arm7_timer = nds_timer;
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
	if (cheats)
		cheats->process();
}

void execHardware_interrupts()
{
	if((MMU.reg_IF[0]&MMU.reg_IE[0]) && (MMU.reg_IME[0]))
	{
#ifdef GDB_STUB
		if ( armcpu_flagIrq( &NDS_ARM9)) 
#else
		if ( armcpu_irqException(&NDS_ARM9))
#endif
		{
			//printf("ARM9 interrupt! flags: %08X ; mask: %08X ; result: %08X\n",MMU.reg_IF[0],MMU.reg_IE[0],MMU.reg_IF[0]&MMU.reg_IE[0]);
			//nds.ARM9Cycle = nds.cycles;
		}
	}

	if((MMU.reg_IF[1]&MMU.reg_IE[1]) && (MMU.reg_IME[1]))
	{
#ifdef GDB_STUB
		if ( armcpu_flagIrq( &NDS_ARM7)) 
#else
		if ( armcpu_irqException(&NDS_ARM7))
#endif
		{
			//nds.ARM7Cycle = nds.cycles;
		}
	}
}

static void resetUserInput();

bool _HACK_DONT_STOPMOVIE = false;
void NDS_Reset()
{
	singleStep = false;
	nds_debug_continuing[0] = nds_debug_continuing[1] = false;
	u32 src = 0;
	u32 dst = 0;
	bool fw_success = false;
	FILE* inf = NULL;
	NDS_header * header = NDS_getROMHeader();

	DEBUG_reset();

	if (!header) return ;

	nds.sleeping = FALSE;
	nds.cardEjected = FALSE;
	nds.power1.lcd = nds.power1.gpuMain = nds.power1.gfx3d_render = nds.power1.gfx3d_geometry = nds.power1.gpuSub = nds.power1.dispswap = 1;
	nds.power2.speakers = 1;
	nds.power2.wifi = 0;

	nds_timer = 0;
	nds_arm9_timer = 0;
	nds_arm7_timer = 0;

	if(movieMode != MOVIEMODE_INACTIVE && !_HACK_DONT_STOPMOVIE)
		movie_reset_command = true;

	if(movieMode == MOVIEMODE_INACTIVE) {
		currFrameCounter = 0;
		lagframecounter = 0;
		LagFrameFlag = 0;
		lastLag = 0;
		TotalLagFrames = 0;
	}

	MMU_Reset();

	//ARM7 BIOS IRQ HANDLER
	if(CommonSettings.UseExtBIOS == true)
		inf = fopen(CommonSettings.ARM7BIOS,"rb");
	else
		inf = NULL;

	if(inf) 
	{
		fread(MMU.ARM7_BIOS,1,16384,inf);
		fclose(inf);

		if(CommonSettings.SWIFromBIOS == true) NDS_ARM7.swi_tab = 0;
		else NDS_ARM7.swi_tab = ARM7_swi_tab;

		if (CommonSettings.PatchSWI3)
			_MMU_write16<ARMCPU_ARM7>(0x00002F08, 0x4770);

		INFO("ARM7 BIOS is loaded.\n");
	} 
	else 
	{
		NDS_ARM7.swi_tab = ARM7_swi_tab;

		for (int t = 0; t < 16384; t++)
			MMU.ARM7_BIOS[t] = 0xFF;

		T1WriteLong(MMU.ARM7_BIOS,0x00, 0xE25EF002);
		T1WriteLong(MMU.ARM7_BIOS,0x04, 0xEAFFFFFE);
		T1WriteLong(MMU.ARM7_BIOS,0x18, 0xEA000000);
		T1WriteLong(MMU.ARM7_BIOS,0x20, 0xE92D500F);
		T1WriteLong(MMU.ARM7_BIOS,0x24, 0xE3A00301);
		T1WriteLong(MMU.ARM7_BIOS,0x28, 0xE28FE000);
		T1WriteLong(MMU.ARM7_BIOS,0x2C, 0xE510F004);
		T1WriteLong(MMU.ARM7_BIOS,0x30, 0xE8BD500F);
		T1WriteLong(MMU.ARM7_BIOS,0x34, 0xE25EF004);
	}

	//ARM9 BIOS IRQ HANDLER
	if(CommonSettings.UseExtBIOS == true)
		inf = fopen(CommonSettings.ARM9BIOS,"rb");
	else
		inf = NULL;
	//memcpy(MMU.ARM9_BIOS + 0x20, gba_header_data_0x04, 156);

	if(inf) 
	{
		fread(MMU.ARM9_BIOS,1,4096,inf);
		fclose(inf);

		if(CommonSettings.SWIFromBIOS == true) NDS_ARM9.swi_tab = 0;
		else NDS_ARM9.swi_tab = ARM9_swi_tab;

		if (CommonSettings.PatchSWI3)
			_MMU_write16<ARMCPU_ARM9>(0xFFFF07CC, 0x4770);

		INFO("ARM9 BIOS is loaded.\n");
	} 
	else 
	{
		NDS_ARM9.swi_tab = ARM9_swi_tab;

		for (int t = 0; t < 4096; t++)
			MMU.ARM9_BIOS[t] = 0xFF;

		_MMU_write32<ARMCPU_ARM9>(0xFFFF0018, 0xEA000095);

		for (int t = 0; t < 156; t++)		// load logo
			MMU.ARM9_BIOS[t + 0x20] = logo_data[t];

		_MMU_write32<ARMCPU_ARM9>(0xFFFF0274, 0xE92D500F);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF0278, 0xEE190F11);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF027C, 0xE1A00620);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF0280, 0xE1A00600);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF0284, 0xE2800C40);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF0288, 0xE28FE000);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF028C, 0xE510F004);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF0290, 0xE8BD500F);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF0294, 0xE25EF004);
	}

	if (firmware)
	{
		delete firmware;
		firmware = NULL;
	}
	firmware = new CFIRMWARE();
	fw_success = firmware->load();
	if ((CommonSettings.UseExtBIOS == true) && (CommonSettings.BootFromFirmware == true) && (fw_success == TRUE))
	{
		// Copy secure area to memory if needed
		if ((header->ARM9src >= 0x4000) && (header->ARM9src < 0x8000))
		{
			src = header->ARM9src;
			dst = header->ARM9cpy;

			u32 size = (0x8000 - src) >> 2;
			//INFO("Copy secure area from 0x%08X to 0x%08X (size %i/0x%08X)\n", src, dst, size, size);
			for (u32 i = 0; i < size; i++)
			{
				_MMU_write32<ARMCPU_ARM9>(dst, T1ReadLong(MMU.CART_ROM, src));
				src += 4; dst += 4;
			}
		}

		if (firmware->patched)
		{
			armcpu_init(&NDS_ARM7, 0x00000008);
			armcpu_init(&NDS_ARM9, 0xFFFF0008);
		}
		else
		{
			//INFO("Booting at ARM9: 0x%08X, ARM7: 0x%08X\n", firmware->ARM9bootAddr, firmware->ARM7bootAddr);
			// need for firmware
			//armcpu_init(&NDS_ARM7, 0x00000008);
			//armcpu_init(&NDS_ARM9, 0xFFFF0008);
			armcpu_init(&NDS_ARM7, firmware->ARM7bootAddr);
			armcpu_init(&NDS_ARM9, firmware->ARM9bootAddr);
		}

			_MMU_write08<ARMCPU_ARM9>(0x04000300, 0);
			_MMU_write08<ARMCPU_ARM7>(0x04000300, 0);
	}
	else
	{
		src = header->ARM9src;
		dst = header->ARM9cpy;

		for(u32 i = 0; i < (header->ARM9binSize>>2); ++i)
		{
			_MMU_write32<ARMCPU_ARM9>(dst, T1ReadLong(MMU.CART_ROM, src));
			dst += 4;
			src += 4;
		}

		src = header->ARM7src;
		dst = header->ARM7cpy;

		for(u32 i = 0; i < (header->ARM7binSize>>2); ++i)
		{
			_MMU_write32<ARMCPU_ARM7>(dst, T1ReadLong(MMU.CART_ROM, src));
			dst += 4;
			src += 4;
		}

		armcpu_init(&NDS_ARM7, header->ARM7exe);
		armcpu_init(&NDS_ARM9, header->ARM9exe);
		
		_MMU_write08<ARMCPU_ARM9>(0x04000300, 1);
		_MMU_write08<ARMCPU_ARM7>(0x04000300, 1);
	}
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
	NDS_ARM9.R[13] = NDS_ARM9.R13_usr;
	//n.b.: im not sure about all these, I dont know enough about arm9 svc/irq/etc modes
	//and how theyre named in desmume to match them up correctly. i just guessed.

	nds.wifiCycle = 0;
	memset(nds.timerCycle, 0, sizeof(u64) * 2 * 4);
	nds.VCount = 0;
	nds.old = 0;
	nds.touchX = nds.touchY = 0;
	nds.isTouch = 0;
	nds.debugConsole = CommonSettings.DebugConsole;
	nds.ensataEmulation = CommonSettings.EnsataEmulation;
	nds.ensataHandshake = ENSATA_HANDSHAKE_none;
	nds.ensataIpcSyncCounter = 0;
	SetupMMU(nds.debugConsole);

	_MMU_write16<ARMCPU_ARM9>(0x04000130, 0x3FF);
	_MMU_write16<ARMCPU_ARM7>(0x04000130, 0x3FF);
	_MMU_write08<ARMCPU_ARM7>(0x04000136, 0x43);

	//picross polls this value when it boots. firmware sets it.
	//_MMU_write16<ARMCPU_ARM9>(REG_POWCNT1, 0x20F);

	LidClosed = FALSE;
	countLid = 0;

	resetUserInput();

	//Setup a copy of the firmware user settings in memory.
	//(this is what the DS firmware would do).
	{
		u8 temp_buffer[NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT];
		int fw_index;

		if ( copy_firmware_user_data( temp_buffer, MMU.fw.data)) {
			for ( fw_index = 0; fw_index < NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT; fw_index++)
				_MMU_write08<ARMCPU_ARM9>(0x027FFC80 + fw_index, temp_buffer[fw_index]);
		}
	}

	// Copy the whole header to Main RAM 0x27FFE00 on startup.
	//  Reference: http://nocash.emubase.de/gbatek.htm#dscartridgeheader
	//zero 27-jun-09 : why did this copy 0x90 more? gbatek says its not stored in ram.
	//for (i = 0; i < ((0x170+0x90)/4); i++) {
	for (int i = 0; i < ((0x170)/4); i++)
		_MMU_write32<ARMCPU_ARM9>(0x027FFE00+i*4, LE_TO_LOCAL_32(((u32*)MMU.CART_ROM)[i]));

	// Write the header checksum to memory (the firmware needs it to see the cart)
	_MMU_write16<ARMCPU_ARM9>(0x027FF808, T1ReadWord(MMU.CART_ROM, 0x15E));

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
	
	// 
	_MMU_write32<ARMCPU_ARM9>(0x02FFFE70, 0x5f617267);
	_MMU_write32<ARMCPU_ARM9>(0x02FFFE74, kCommandline); //(commandline starts here)
	_MMU_write32<ARMCPU_ARM9>(0x02FFFE78, rompath.size()+1);
	//0x027FFF7C (argc)
	//0x027FFF80 (argv)
	for(size_t i=0;i<rompath.size();i++)
		_MMU_write08<ARMCPU_ARM9>(kCommandline+i, rompath[i]);
	_MMU_write08<ARMCPU_ARM9>(kCommandline+rompath.size(), 0);
	//--------------------------------

	if ((firmware->patched) && (CommonSettings.UseExtBIOS == true) && (CommonSettings.BootFromFirmware == true) && (fw_success == TRUE))
	{
		// HACK! for flashme
		_MMU_write32<ARMCPU_ARM9>(0x27FFE24, firmware->ARM9bootAddr);
		_MMU_write32<ARMCPU_ARM7>(0x27FFE34, firmware->ARM7bootAddr);
	}


	// Save touchscreen calibration info in a structure
	// so we can easily access it at any time
	TSCal.adc.x1 = _MMU_read16<ARMCPU_ARM7>(0x027FFC80 + 0x58);
	TSCal.adc.y1 = _MMU_read16<ARMCPU_ARM7>(0x027FFC80 + 0x5A);
	TSCal.scr.x1 = _MMU_read08<ARMCPU_ARM7>(0x027FFC80 + 0x5C);
	TSCal.scr.y1 = _MMU_read08<ARMCPU_ARM7>(0x027FFC80 + 0x5D);
	TSCal.adc.x2 = _MMU_read16<ARMCPU_ARM7>(0x027FFC80 + 0x5E);
	TSCal.adc.y2 = _MMU_read16<ARMCPU_ARM7>(0x027FFC80 + 0x60);
	TSCal.scr.x2 = _MMU_read08<ARMCPU_ARM7>(0x027FFC80 + 0x62);
	TSCal.scr.y2 = _MMU_read08<ARMCPU_ARM7>(0x027FFC80 + 0x63);

	TSCal.adc.width = (TSCal.adc.x2 - TSCal.adc.x1);
	TSCal.adc.height = (TSCal.adc.y2 - TSCal.adc.y1);
	TSCal.scr.width = (TSCal.scr.x2 - TSCal.scr.x1);
	TSCal.scr.height = (TSCal.scr.y2 - TSCal.scr.y1);

	MainScreen.offset = 0;
	SubScreen.offset = 192;

	//_MMU_write32[ARMCPU_ARM9](0x02007FFC, 0xE92D4030);

	delete header;

	Screen_Reset();
	gfx3d_reset();
	gpu3D->NDS_3D_Reset();
	SPU_Reset();

	WIFI_Reset();

	memcpy(FW_Mac, (MMU.fw.data + 0x36), 6);

	initSchedule();
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
buttonstruct<int> TurboTime;
buttonstruct<bool> AutoHold;

void ClearAutoHold(void) {
	
	for (u32 i=0; i < ARRAY_SIZE(AutoHold.array); i++) {
		AutoHold.array[i]=false;
	}
}


INLINE u16 NDS_getADCTouchPosX(u16 scrX)
{
	// this is a little iffy,
	// we're basically adjusting the ADC results to
	// compensate for how they will be interpreted.
	// the actual system doesn't do this transformation.
	int rv = (scrX - TSCal.scr.x1 + 1) * TSCal.adc.width / TSCal.scr.width + TSCal.adc.x1;
	rv = min(0xFFF, max(0, rv));
	return (u16)rv;
}
INLINE u16 NDS_getADCTouchPosY(u16 scrY)
{
	int rv = (scrY - TSCal.scr.y1 + 1) * TSCal.adc.height / TSCal.scr.height + TSCal.adc.y1;
	rv = min(0xFFF, max(0, rv));
	return (u16)rv;
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


static void saveUserInput(EMUFILE* os, UserInput& input)
{
	os->fwrite((const char*)input.buttons.array, 14);
	writebool(input.touch.isTouch, os);
	write16le(input.touch.touchX, os);
	write16le(input.touch.touchY, os);
	write32le(input.mic.micButtonPressed, os);
}
static bool loadUserInput(EMUFILE* is, UserInput& input, int version)
{
	is->fread((char*)input.buttons.array, 14);
	readbool(&input.touch.isTouch, is);
	read16le(&input.touch.touchX, is);
	read16le(&input.touch.touchY, is);
	read32le(&input.mic.micButtonPressed, is);
	return true;
}
static void resetUserInput(UserInput& input)
{
	memset(&input, 0, sizeof(UserInput));
}
// (userinput is kind of a misnomer, e.g. finalUserInput has to mirror nds.pad, nds.touchX, etc.)
static void saveUserInput(EMUFILE* os)
{
	saveUserInput(os, finalUserInput);
	saveUserInput(os, intermediateUserInput); // saved in case a savestate is made during input processing (which Lua could do if nothing else)
	writebool(validToProcessInput, os);
	for(int i = 0; i < 14; i++)
		write32le(TurboTime.array[i], os); // saved to make autofire more tolerable to use with re-recording
}
static bool loadUserInput(EMUFILE* is, int version)
{
	bool rv = true;
	rv &= loadUserInput(is, finalUserInput, version);
	rv &= loadUserInput(is, intermediateUserInput, version);
	readbool(&validToProcessInput, is);
	for(int i = 0; i < 14; i++)
		read32le((u32*)&TurboTime.array[i], is);
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
	rawUserInput.touch.touchX = NDS_getADCTouchPosX(x);
	rawUserInput.touch.touchY = NDS_getADCTouchPosY(y);
	rawUserInput.touch.isTouch = true;

	if(movieMode != MOVIEMODE_INACTIVE && movieMode != MOVIEMODE_FINISHED)
	{
		// just in case, since the movie only stores 8 bits per touch coord
		rawUserInput.touch.touchX &= 0x0FF0;
		rawUserInput.touch.touchY &= 0x0FF0;
	}

#ifndef WIN32
	// FIXME: this code should be deleted from here,
	// other platforms should call NDS_beginProcessingInput,NDS_endProcessingInput once per frame instead
	// (see the function called "StepRunLoop_Core" in src/windows/main.cpp),
	// but I'm leaving this here for now since I can't test those other platforms myself.
	nds.touchX = rawUserInput.touch.touchX;
	nds.touchY = rawUserInput.touch.touchY;
	nds.isTouch = 1;
	MMU.ARM7_REG[0x136] &= 0xBF;
#endif
}
void NDS_releaseTouch(void)
{ 
	gotInputRequest();
	rawUserInput.touch.touchX = 0;
	rawUserInput.touch.touchY = 0;
	rawUserInput.touch.isTouch = false;

#ifndef WIN32
	// FIXME: this code should be deleted from here,
	// other platforms should call NDS_beginProcessingInput,NDS_endProcessingInput once per frame instead
	// (see the function called "StepRunLoop_Core" in src/windows/main.cpp),
	// but I'm leaving this here for now since I can't test those other platforms myself.
	nds.touchX = 0;
	nds.touchY = 0;
	nds.isTouch = 0;
	MMU.ARM7_REG[0x136] |= 0x40;
#endif
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

	u16	pad	= (0 |
		((input.buttons.A ? 0 : 0x80) >> 7) |
		((input.buttons.B ? 0 : 0x80) >> 6) |
		((input.buttons.T ? 0 : 0x80) >> 5) |
		((input.buttons.S ? 0 : 0x80) >> 4) |
		((input.buttons.R ? 0 : 0x80) >> 3) |
		((input.buttons.L ? 0 : 0x80) >> 2) |
		((input.buttons.U ? 0 : 0x80) >> 1) |
		((input.buttons.D ? 0 : 0x80)     ) |
		((input.buttons.E ? 0 : 0x80) << 1) |
		((input.buttons.W ? 0 : 0x80) << 2)) ;

	((u16 *)MMU.ARM9_REG)[0x130>>1] = (u16)pad;
	((u16 *)MMU.ARM7_REG)[0x130>>1] = (u16)pad;

	u16 k_cnt = ((u16 *)MMU.ARM9_REG)[0x132>>1];
	if ( k_cnt & (1<<14))
	{
		//INFO("ARM9: KeyPad IRQ (pad 0x%04X, cnt 0x%04X (condition %s))\n", pad, k_cnt, k_cnt&(1<<15)?"AND":"OR");
		u16 k_cnt_selected = (k_cnt & 0x3F);
		if (k_cnt&(1<<15))	// AND
		{
			if ((~pad & k_cnt_selected) == k_cnt_selected) NDS_makeARM9Int(12);
		}
		else				// OR
		{
			if (~pad & k_cnt_selected) NDS_makeARM9Int(12);
		}
	}

	k_cnt = ((u16 *)MMU.ARM7_REG)[0x132>>1];
	if ( k_cnt & (1<<14))
	{
		//INFO("ARM7: KeyPad IRQ (pad 0x%04X, cnt 0x%04X (condition %s))\n", pad, k_cnt, k_cnt&(1<<15)?"AND":"OR");
		u16 k_cnt_selected = (k_cnt & 0x3F);
		if (k_cnt&(1<<15))	// AND
		{
			if ((~pad & k_cnt_selected) == k_cnt_selected) NDS_makeARM7Int(12);
		}
		else				// OR
		{
			if (~pad & k_cnt_selected) NDS_makeARM7Int(12);
		}
	}


	if(input.touch.isTouch)
	{
		nds.touchX = input.touch.touchX;
		nds.touchY = input.touch.touchY;
		nds.isTouch = 1;

		MMU.ARM7_REG[0x136] &= 0xBF;
	}
	else
	{
		nds.touchX = 0;
		nds.touchY = 0;
		nds.isTouch = 0;

		MMU.ARM7_REG[0x136] |= 0x40;
	}


	if (input.buttons.F && !countLid) 
	{
		LidClosed = (!LidClosed) & 0x01;
		if (!LidClosed)
		{
		//	SPU_Pause(FALSE);
			NDS_makeARM7Int(22);

		}
		//else
			//SPU_Pause(TRUE);

		countLid = 30;
	}
	else 
	{
		if (countLid > 0)
			countLid--;
	}

	u16 padExt = (((u16 *)MMU.ARM7_REG)[0x136>>1] & 0x0070) |
		((input.buttons.X ? 0 : 0x80) >> 7) |
		((input.buttons.Y ? 0 : 0x80) >> 6) |
		((input.buttons.G ? 0 : 0x80) >> 4) |
		((LidClosed) << 7) |
		0x0034;

	((u16 *)MMU.ARM7_REG)[0x136>>1] = (u16)padExt;

	InputDisplayString=MakeInputDisplayString(padExt, pad);

	//put into the format we want for the movie system
	//fRLDUTSBAYXWEg
	//we don't really need nds.pad anymore, but removing it would be a pain

 	nds.pad =
		((input.buttons.R ? 1 : 0) << 12)|
		((input.buttons.L ? 1 : 0) << 11)|
		((input.buttons.D ? 1 : 0) << 10)|
		((input.buttons.U ? 1 : 0) << 9)|
		((input.buttons.T ? 1 : 0) << 8)|
		((input.buttons.S ? 1 : 0) << 7)|
		((input.buttons.B ? 1 : 0) << 6)|
		((input.buttons.A ? 1 : 0) << 5)|
		((input.buttons.Y ? 1 : 0) << 4)|
		((input.buttons.X ? 1 : 0) << 3)|
		((input.buttons.W ? 1 : 0) << 2)|
		((input.buttons.E ? 1 : 0) << 1);

	// TODO: low power IRQ
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


void emu_halt() {
	//printf("halting emu: ARM9 PC=%08X/%08X, ARM7 PC=%08X/%08X\n", NDS_ARM9.R[15], NDS_ARM9.instruct_adr, NDS_ARM7.R[15], NDS_ARM7.instruct_adr);
	execute = false;
}

//these templates needed to be instantiated manually
template void NDS_exec<FALSE>(s32 nb);
template void NDS_exec<TRUE>(s32 nb);
