/*
	Copyright (C) 2006 thoduv
	Copyright (C) 2006-2007 Theo Berkau
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

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "debug.h"
#include "mc.h"
#include "movie.h"
#include "readwrite.h"
#include "NDSSystem.h"
#include "path.h"
#include "utils/advanscene.h"

//#define _DONT_LOAD_BACKUP
//#define _DONT_SAVE_BACKUP

// TODO: motion device was broken
//#define _ENABLE_MOTION

#define BM_CMD_NOP					0x00
#define BM_CMD_AUTODETECT			0xFF
#define BM_CMD_WRITESTATUS			0x01
#define BM_CMD_WRITELOW				0x02
#define BM_CMD_READLOW				0x03
#define BM_CMD_WRITEDISABLE			0x04
#define BM_CMD_READSTATUS			0x05
#define BM_CMD_WRITEENABLE			0x06
#define BM_CMD_WRITEHIGH			0x0A
#define BM_CMD_READHIGH				0x0B

// Pokemons IrDA
#define BM_CMD_IRDA					0x08

//FLASH
#define COMM_PAGE_WRITE				0x0A
#define COMM_PAGE_ERASE				0xDB
#define COMM_SECTOR_ERASE			0xD8
#define COMM_CHIP_ERASE				0xC7
#define CARDFLASH_READ_BYTES_FAST	0x0B    /* Not used*/
#define CARDFLASH_DEEP_POWDOWN		0xB9    /* Not used*/
#define CARDFLASH_WAKEUP			0xAB    /* Not used*/

//since r2203 this was 0x00.
//but baby pals proves finally that it should be 0xFF:
//the game reads its initial sound volumes from uninitialized data, and if it is 0, the game will be silent
//if it is 0xFF then the game starts with its sound and music at max, as presumably it is supposed to.
//so in r3303 I finally changed it (no$ appears definitely to initialize to 0xFF)
static u8 kUninitializedSaveDataValue = 0xFF;

static const char* kDesmumeSaveCookie = "|-DESMUME SAVE-|";

static const u32 saveSizes[] = {512,			// 4k
								8*1024,			// 64k
								32*1024,		// 512k
								64*1024,		// 1Mbit
								256*1024,		// 2Mbit
								512*1024,		// 4Mbit
								1024*1024,		// 8Mbit
								2048*1024,		// 16Mbit
								4096*1024,		// 32Mbit
								8192*1024,		// 64Mbit
								16384*1024,		// 128Mbit
								32768*1024,		// 256Mbit
								65536*1024,		// 512Mbit
								0xFFFFFFFF};
static const u32 saveSizes_count = ARRAY_SIZE(saveSizes);

//the lookup table from user save types to save parameters
const SAVE_TYPE save_types[] = {
	{"Autodetect",		MC_TYPE_AUTODETECT,	1},
	{"EEPROM 4kbit",	MC_TYPE_EEPROM1,	MC_SIZE_4KBITS},
	{"EEPROM 64kbit",	MC_TYPE_EEPROM2,	MC_SIZE_64KBITS},
	{"EEPROM 512kbit",	MC_TYPE_EEPROM2,	MC_SIZE_512KBITS},
	{"FRAM 256kbit",	MC_TYPE_FRAM,		MC_SIZE_256KBITS},
	{"FLASH 2Mbit",		MC_TYPE_FLASH,		MC_SIZE_2MBITS},
	{"FLASH 4Mbit",		MC_TYPE_FLASH,		MC_SIZE_4MBITS},
	{"FLASH 8Mbit",		MC_TYPE_FLASH,		MC_SIZE_8MBITS},
	{"FLASH 16Mbit",	MC_TYPE_FLASH,		MC_SIZE_16MBITS},
	{"FLASH 32Mbit",	MC_TYPE_FLASH,		MC_SIZE_32MBITS},
	{"FLASH 64Mbit",	MC_TYPE_FLASH,		MC_SIZE_64MBITS},
	{"FLASH 128Mbit",	MC_TYPE_FLASH,		MC_SIZE_128MBITS},
	{"FLASH 256Mbit",	MC_TYPE_FLASH,		MC_SIZE_256MBITS},
	{"FLASH 512Mbit",	MC_TYPE_FLASH,		MC_SIZE_512MBITS}
};


//forces the currently selected backup type to be current
//(can possibly be used to repair poorly chosen save types discovered late in gameplay i.e. pokemon gamers)
void backup_forceManualBackupType()
{
	MMU_new.backupDevice.forceManualBackupType();
}

void backup_setManualBackupType(int type)
{
	CommonSettings.manualBackupType = type;
}

bool BackupDevice::save_state(EMUFILE* os)
{
	u32 version = 4;
	//v0
	write32le(version,os);
	write32le(write_enable,os);
	write32le(com,os);
	write32le(addr_size,os);
	write32le(addr_counter,os);
	write32le((u32)state,os);
	writebuffer(data,os);
	writebuffer(data_autodetect,os);
	//v1
	write32le(addr,os);
	//v2
	write8le(motionInitState,os);
	write8le(motionFlag,os);
	//v3
	writebool(reset_command_state,os);
	//v4
	write8le(write_protect,os);
	return true;
}

bool BackupDevice::load_state(EMUFILE* is)
{
	u32 version;
	if(read32le(&version,is)!=1) return false;
	if(version>=0)
	{
		readbool(&write_enable,is);
		read32le(&com,is);
		read32le(&addr_size,is);
		read32le(&addr_counter,is);
		u32 temp;
		read32le(&temp,is);
		state = (STATE)temp;
		readbuffer(data,is);
		readbuffer(data_autodetect,is);
	}
	if(version>=1)
		read32le(&addr,is);
	
	if(version>=2)
	{
		read8le(&motionInitState,is);
		read8le(&motionFlag,is);
	}

	if(version>=3)
	{
		readbool(&reset_command_state,is);
	}

	if(version>=4)
	{
		read8le(&write_protect,is);
	}

	return true;
}

BackupDevice::BackupDevice()
{
	char buf[MAX_PATH] = {0};
	memset(buf, 0, MAX_PATH);
	path.getpathnoext(path.BATTERY, buf);
	filename = std::string(buf) + ".dsv";						// DeSmuME memory card

	isMovieMode = false;
	reset();
}

void BackupDevice::movie_mode()
{
	isMovieMode = true;
	reset();
}

void BackupDevice::reset_hardware()
{
	write_enable = FALSE;
	write_protect = 0;
	com = 0;
	addr = addr_counter = 0;
	motionInitState = MOTION_INIT_STATE_IDLE;
	motionFlag = MOTION_FLAG_NONE;
	state = DETECTING;
	reset_command_state = false;
	flushPending = false;
	lazyFlushPending = false;

	kUninitializedSaveDataValue = 0xFF;

	if(!memcmp(gameInfo.header.gameCode,"AXBJ", 4)) kUninitializedSaveDataValue = 0x00; // Daigassou! Band Brothers DX (JP)
}

void BackupDevice::reset()
{
	memset(&info, 0, sizeof(info));
	reset_hardware();
	resize(0);
	data_autodetect.resize(0);
	addr_size = 0;
	loadfile();

	//if the user has requested a manual choice for backup type, and we havent imported a raw save file, then apply it now
	if(state == DETECTING && CommonSettings.manualBackupType != MC_TYPE_AUTODETECT)
	{
		state = RUNNING;
		int savetype = save_types[CommonSettings.manualBackupType].media_type;
		int savesize = save_types[CommonSettings.manualBackupType].size;
		ensure((u32)savesize); //expand properly if necessary
		resize(savesize); //truncate if necessary
		addr_size = addr_size_for_old_save_type(savetype);
		flush();
	}
}

void BackupDevice::close_rom()
{
	flush();
}

u8 BackupDevice::searchFileSaveType(u32 size)
{
	for (u8 i = 1; i < MAX_SAVE_TYPES; i++)
	{
		if (size == save_types[i].size)
			return (i-1);
	}
	return 0xFF;
}

void BackupDevice::detect()
{
	if (!reset_command_state) return;

	if(state == DETECTING && data_autodetect.size()>0)
	{
		//we can now safely detect the save address size
		u32 autodetect_size = data_autodetect.size();

		printf("Autodetecting with autodetect_size=%d\n",autodetect_size);

		//detect based on rules
		switch(autodetect_size)
		{
			case 0:
			case 1:
				addr_size = 1; //choose 1 just to keep the busted savefile from growing too big
				msgbox->error("Catastrophic error while autodetecting save type.\nIt will need to be specified manually\n");
				break;
			case 2:
				 //the modern typical case for small eeproms
				addr_size = 1;
				break;
			case 3:
				//another modern typical case..
				//but unfortunately we select this case on accident sometimes when what it meant to do was present the archaic 1+2 case
				//(the archaic 1+2 case is: specifying one address byte, and then reading the first two bytes, instead of the first one byte, as most other games would do.)
				//so, we're gonna hack in checks for the games that are doing this
				addr_size = 2;

				// TODO: will study a deep, why this happens (wrong detect size)
				if(!memcmp(gameInfo.header.gameCode,"AL3", 3)) addr_size = 1; //spongebob atlantis squarepantis.
				if(!memcmp(gameInfo.header.gameCode,"AH5", 3)) addr_size = 1; //over the hedge
				if(!memcmp(gameInfo.header.gameCode,"AVH", 3)) addr_size = 1; //over the hedge - Hammy Goes Nuts!
				if(!memcmp(gameInfo.header.gameCode,"AQ3", 3)) addr_size = 1; //spider-man 3

				break;
			case 4:
				//a modern typical case
				addr_size = 3;
				if(!memcmp(gameInfo.header.gameCode,"ASM", 3)) addr_size = 2; //super mario 64 ds
				break;
			default:
				//the archaic case: write the address and then some modulo-4 number of bytes
				//why modulo 4? who knows.
				//SM64 (KOR) makes it here with autodetect_size=11 and nothing interesting in the buffer
				addr_size = autodetect_size & 3;

				if(!memcmp(gameInfo.header.gameCode,"BDE", 3)) addr_size = 2; // Dementium II
				break;
		}

		state = RUNNING;
		data_autodetect.resize(0);
		flush();
	}
}

void BackupDevice::checkReset()
{
	if (reset_command_state)
	{
		//printf("MC  : reset command\n");

		//for a performance hack, save files are only flushed after each reset command
		//(hopefully, after each page)
		if(flushPending && (com == BM_CMD_WRITELOW || com == BM_CMD_WRITEHIGH))
		{
			flush();
			flushPending = false;
			lazyFlushPending = false;
		}
		
		com = 0;
		reset_command_state = false;
	}
}

u8 BackupDevice::data_command(u8 val, u8 PROCNUM)
{
	//printf("MC%c : cmd %02X, val %02X\n", PROCNUM?'7':'9', com, val);

#ifdef _ENABLE_MOTION
	//motion: some guessing here... hope it doesn't break anything
	if(com == BM_CMD_READLOW && motionInitState == MOTION_INIT_STATE_RECEIVED_4_B && val == 0)
	{
		motionInitState = MOTION_INIT_STATE_IDLE;
		motionFlag |= MOTION_FLAG_ENABLED;
		//return 0x04; //return 0x04 to enable motion!!!!!
		return 0; //but we return 0 to disable it! since we don't emulate it anyway
	}

	//if the game firmly believes we have motion support, then ignore all motion commands, since theyre not emulated.
	if(motionFlag & MOTION_FLAG_SENSORMODE)
	{
		return 0;
	}
#endif

	switch (com)
	{
		case BM_CMD_WRITESTATUS:
			//printf("MC%c: write status %02X\n", PROCNUM?'7':'9', val);
			write_protect = (val & 0xFC);
		break;

		case BM_CMD_WRITELOW:
		case BM_CMD_READLOW:
			//handle data or address
			if(state == DETECTING)
			{
				if(com == BM_CMD_WRITELOW)
					printf("MC%c: Unexpected backup device initialization sequence using writes!\n", PROCNUM?'7':'9');

				//just buffer the data until we're no longer detecting
				data_autodetect.push_back(val);
				detect();
				val = 0xFF;
			}
			else
			{
				if(addr_counter<addr_size)
				{
					//continue building address
					addr <<= 8;
					addr |= val;
					addr_counter++;
					val = 0xFF;
				}
				else
				{
					//why does tomb raider underworld access 0x180 and go clear through to 0x280?
					//should this wrap around at 0 or at 0x100?
					if(addr_size == 1) addr &= 0x1FF; 

					//address is complete
					ensure(addr+1);
					if(com == BM_CMD_READLOW)
					{
						val = data[addr];
						//flushPending = true; //is this a good idea? it may slow stuff down, but it is helpful for debugging
						lazyFlushPending = true; //lets do this instead
						//printf("MC%c: read from %08X, value %02Xh\n", PROCNUM?'7':'9', addr, val);
					}
					else 
						if(write_enable)
						{
							data[addr] = val;
							flushPending = true;
							//printf("MC%c: write to %08X, value %02Xh\n", PROCNUM?'7':'9', addr, val);
						}

					addr++;
				}
			}
		break;
		
		case BM_CMD_READSTATUS:
			//handle request to read status
			//LOG("Backup Memory Read Status: %02X\n", write_enable << 1);
			val = (write_enable << 1) | write_protect;
		break;

		case BM_CMD_IRDA:
			printf("MC%c: Unverified Backup Memory command: %02X FROM %08X\n", PROCNUM?'7':'9', com, PROCNUM?NDS_ARM7.instruct_adr:NDS_ARM9.instruct_adr);
			val = 0xAA;
		break;

		default:
			if (com != 0)
			{
				printf("MC%c: Unhandled Backup Memory command %02X, value %02X (PC:%08X)\n", PROCNUM?'7':'9', com, val, PROCNUM?NDS_ARM7.instruct_adr:NDS_ARM9.instruct_adr);
				break;
			}

			com = val;
			val = 0xFF;

			//there is no current command. receive one
			switch (com)
			{
				case BM_CMD_NOP: break;

				case BM_CMD_WRITESTATUS: break;

#ifdef _ENABLE_MOTION
				case 0xFE:
					if(motionInitState == MOTION_INIT_STATE_IDLE) { motionInitState = MOTION_INIT_STATE_FE; return 0; }
					break;
				case 0xFD:
					if(motionInitState == MOTION_INIT_STATE_FE) { motionInitState = MOTION_INIT_STATE_FD; return 0; }
					break;
				case 0xFB:
					if(motionInitState == MOTION_INIT_STATE_FD) { motionInitState = MOTION_INIT_STATE_FB; return 0; }
					break;
				case 0xF8:
					//enable sensor mode
					if(motionInitState == MOTION_INIT_STATE_FD) 
					{
						motionInitState = MOTION_INIT_STATE_IDLE;
						motionFlag |= MOTION_FLAG_SENSORMODE;
						return 0;
					}
					break;
				case 0xF9:
					//disable sensor mode
					if(motionInitState == MOTION_INIT_STATE_FD) 
					{
						motionInitState = MOTION_INIT_STATE_IDLE;
						motionFlag &= ~MOTION_FLAG_SENSORMODE;
						return 0;
					}
					break;
#endif

				case BM_CMD_IRDA:
					printf("MC%c: Unverified Backup Memory command: %02X FROM %08X\n", PROCNUM?'7':'9', com, PROCNUM?NDS_ARM7.instruct_adr:NDS_ARM9.instruct_adr);
					
					val = 0xAA;
					break;

				case BM_CMD_WRITEDISABLE:
					//printf("MC%c: write disable\n", PROCNUM?'7':'9');
#ifdef _ENABLE_MOTION
					switch (motionInitState)
					{
						case MOTION_INIT_STATE_IDLE: motionInitState = MOTION_INIT_STATE_RECEIVED_4; break;
						case MOTION_INIT_STATE_RECEIVED_4: motionInitState = MOTION_INIT_STATE_RECEIVED_4_B; break;
					}
#endif
					write_enable = FALSE;
					break;
								
				case BM_CMD_READSTATUS: break;

				case BM_CMD_WRITEENABLE:
					//printf("MC%c: write enable\n", PROCNUM?'7':'9');
					write_enable = TRUE;
					break;

				case BM_CMD_WRITELOW:
				case BM_CMD_READLOW:
					//printf("XLO: %08X\n",addr);
					addr_counter = 0;
					addr = 0;
					break;

				case BM_CMD_WRITEHIGH:
				case BM_CMD_READHIGH:
					//printf("XHI: %08X\n",addr);
					if(com == BM_CMD_WRITEHIGH) com = BM_CMD_WRITELOW;
					if(com == BM_CMD_READHIGH) com = BM_CMD_READLOW;
					addr_counter = 0;
					addr = 0;
					if(addr_size==1)
					{
						//"write command that's only available on ST M95040-W that I know of"
						//this makes sense, since this device would only have a 256 bytes address space with writelow
						//and writehigh would allow access to the upper 256 bytes
						//but it was detected in pokemon diamond also during the main save process
						addr = 0x1;
					}
					break;

				default:
					printf("MC%c: Unhandled Backup Memory command: %02X FROM %08X\n", PROCNUM?'7':'9', com, PROCNUM?NDS_ARM7.instruct_adr:NDS_ARM9.instruct_adr);
					break;
			} //switch(val)
		break;
	} //switch (com)

#ifdef _ENABLE_MOTION
	//motion control state machine broke, return to ground
	motionInitState = MOTION_INIT_STATE_IDLE;
#endif

	checkReset();

	return val;
}

//guarantees that the data buffer has room enough for the specified number of bytes
void BackupDevice::ensure(u32 addr)
{
	ensure(addr,kUninitializedSaveDataValue);
}

void BackupDevice::ensure(u32 addr, u8 val)
{
	u32 size = data.size();
	if(size<addr)
	{
		resize(addr, val);
	}
}

void BackupDevice::resize(u32 size)
{
	resize(size,kUninitializedSaveDataValue);
}

void BackupDevice::resize(u32 size, u8 val)
{
	size_t old_size = data.size();
	data.resize(size);
	for(u32 i=old_size;i<size;i++)
		data[i] = val;
}

u32 BackupDevice::addr_size_for_old_save_size(int bupmem_size)
{
	switch(bupmem_size) {
		case MC_SIZE_4KBITS: 
			return 1;
		case MC_SIZE_64KBITS: 
		case MC_SIZE_256KBITS:
		case MC_SIZE_512KBITS:
			return 2;
		case MC_SIZE_1MBITS:
		case MC_SIZE_2MBITS:
		case MC_SIZE_4MBITS:
		case MC_SIZE_8MBITS:
		case MC_SIZE_16MBITS:
		case MC_SIZE_64MBITS:
			return 3;
		default:
			return 0xFFFFFFFF;
	}
}

u32 BackupDevice::addr_size_for_old_save_type(int bupmem_type)
{
	switch(bupmem_type)
	{
		case MC_TYPE_EEPROM1:
			return 1;
		case MC_TYPE_EEPROM2:
		case MC_TYPE_FRAM:
              return 2;
		case MC_TYPE_FLASH:
			return 3;
		default:
			return 0xFFFFFFFF;
	}
}


void BackupDevice::load_old_state(u32 addr_size, u8* data, u32 datasize)
{
	state = RUNNING;
	this->addr_size = addr_size;
	resize(datasize);
	memcpy(&this->data[0],data,datasize);

	//dump back out as a dsv, just to keep things sane
	flush();
}

//======================================================================= Tools
//=======================================================================
//=======================================================================
u32 BackupDevice::importDataSize(const char *filename)
{
	u32 res = 0;
	if (strlen(filename) < 4) return 0;

	if (memcmp(filename + strlen(filename) - 4, ".duc", 4) == 0)
	{
		res = get_save_duc_size(filename);
		if (res == 0xFFFFFFFF) return 0;
		return res;
	}

	res = get_save_nogba_size(filename);
	if (res != 0xFFFFFFFF) return res;

	res = get_save_raw_size(filename);
	if (res != 0xFFFFFFFF) return res;

	return 0;
}

u32 BackupDevice::importData(const char *filename, u32 force_size)
{
	if (strlen(filename) < 4) return 0;

	if (memcmp(filename + strlen(filename) - 4, ".duc", 4) == 0)
		return load_duc(filename, force_size);
	else
		if (load_no_gba(filename, force_size))
			return 1;
		else
			return load_raw(filename, force_size);

	return 0;
}

bool BackupDevice::exportData(const char *filename)
{
	if (strlen(filename) < 4)
		return false;

	if (memcmp(filename + strlen(filename) - 5, ".sav*", 5) == 0)
	{
		char tmp[MAX_PATH];
		memset(tmp, 0, MAX_PATH);
		strcpy(tmp, filename);
		tmp[strlen(tmp)-1] = 0;
		return save_no_gba(tmp);
	}

	if (memcmp(filename + strlen(filename) - 4, ".sav", 4) == 0)
		return save_raw(filename);

	return false;
}

//======================================================================= no$GBA
//=======================================================================
//=======================================================================
const char no_GBA_HEADER_ID[] = "NocashGbaBackupMediaSavDataFile";
const char no_GBA_HEADER_SRAM_ID[] = "SRAM";

u32 BackupDevice::get_save_nogba_size(const char* fname)
{
	FILE *fsrc = fopen(fname, "rb");
	if (fsrc)
	{
		char src[0x50] = {0};
		u32 fsize = 0;
		fseek(fsrc, 0, SEEK_END);
		fsize = ftell(fsrc);
		fseek(fsrc, 0, SEEK_SET);
		if (fsize < 0x50) { fclose(fsrc); return 0xFFFFFFFF; }
		memset(&src[0], 0, sizeof(src));
		if (fread(src, 1, sizeof(src), fsrc) != sizeof(src))  { fclose(fsrc); return 0xFFFFFFFF; }

		for (u8 i = 0; i < 0x1F; i++)
			if (src[i] != no_GBA_HEADER_ID[i]) { fclose(fsrc); return 0xFFFFFFFF; }
		if (src[0x1F] != 0x1A) { fclose(fsrc); return 0xFFFFFFFF; }
		for (int i = 0; i < 0x4; i++)
			if (src[i+0x40] != no_GBA_HEADER_SRAM_ID[i]) { fclose(fsrc); return 0xFFFFFFFF; }
		
		u32 compressMethod = *((u32*)(src+0x44));
		if (compressMethod == 0)
			{ fclose(fsrc); return *((u32*)(src+0x48)); }
		else
			if (compressMethod == 1)
				{ fclose(fsrc); return *((u32*)(src+0x4C)); }
		fclose(fsrc);
	}
	return 0xFFFFFFFF;
}

static int no_gba_unpackSAV(void *in_buf, u32 fsize, void *out_buf, u32 &size)
{
	u8	*src = (u8 *)in_buf;
	u8	*dst = (u8 *)out_buf;
	u32 src_pos = 0;
	u32 dst_pos = 0;
	u8	cc = 0;
	u32	size_unpacked = 0;
	u32	size_packed = 0;
	u32	compressMethod = 0;

	if (fsize < 0x50) return (1);

	for (int i = 0; i < 0x1F; i++)
	{
		if (src[i] != no_GBA_HEADER_ID[i]) return (2);
	}
	if (src[0x1F] != 0x1A) return (2);
	for (int i = 0; i < 0x4; i++)
	{
		if (src[i+0x40] != no_GBA_HEADER_SRAM_ID[i]) return (2);
	}

	compressMethod = *((u32*)(src+0x44));

	if (compressMethod == 0)				// unpacked
	{
		size_unpacked = *((u32*)(src+0x48));
		src_pos = 0x4C;
		for (u32 i = 0; i < size_unpacked; i++)
		{
			dst[dst_pos++] = src[src_pos++];
		}
		size = dst_pos;
		return (0);
	}

	if (compressMethod == 1)				// packed (method 1)
	{
		size_packed = *((u32*)(src+0x48));
		size_unpacked = *((u32*)(src+0x4C));

		src_pos = 0x50;
		while (true)
		{
			cc = src[src_pos++];
			
			if (cc == 0) 
			{
				size = dst_pos;
				return (0);
			}

			if (cc == 0x80)
			{
				u16 tsize = *((u16*)(src+src_pos+1));
				for (int t = 0; t < tsize; t++)
					dst[dst_pos++] = src[src_pos];
				src_pos += 3;
				continue;
			}

			if (cc > 0x80)		// repeat
			{
				cc -= 0x80;
				for (int t = 0; t < cc; t++)
					dst[dst_pos++] = src[src_pos];
				src_pos++;
				continue;
			}
			// copy
			for (int t = 0; t < cc; t++)
				dst[dst_pos++] = src[src_pos++];
		}
		size = dst_pos;
		return (0);
	}
	return (200);
}

static u32 no_gba_savTrim(void *buf, u32 size)
{
	u32 rows = size / 16;
	u32 pos = (size - 16);
	u8	*src = (u8*)buf;

	for (unsigned int i = 0; i < rows; i++, pos -= 16)
	{
		if (src[pos] == 0xFF)
		{
			for (int t = 0; t < 16; t++)
			{
				if (src[pos+t] != 0xFF) return (pos+16);
			}
		}
		else
		{
			return (pos+16);
		}
	}
	return (size);
}

static u32 no_gba_fillLeft(u32 size)
{
	for (u32 i = 1; i < ARRAY_SIZE(save_types); i++)
	{
		if (size <= (u32)save_types[i].size)
			return (size + (save_types[i].size - size));
	}
	return size;
}

bool BackupDevice::load_no_gba(const char *fname, u32 force_size)
{
	FILE	*fsrc = fopen(fname, "rb");
	u8		*in_buf = NULL;
	u8		*out_buf = NULL;

	if (fsrc)
	{
		u32 fsize = 0;
		fseek(fsrc, 0, SEEK_END);
		fsize = ftell(fsrc);
		fseek(fsrc, 0, SEEK_SET);
		//printf("Open %s file (size %i bytes)\n", fname, fsize);

		in_buf = new u8 [fsize];

		if (fread(in_buf, 1, fsize, fsrc) == fsize)
		{
			out_buf = new u8 [8 * 1024 * 1024 / 8];
			u32 size = 0;

			memset(out_buf, 0xFF, 8 * 1024 * 1024 / 8);
			if (no_gba_unpackSAV(in_buf, fsize, out_buf, size) == 0)
			{
				if (force_size > 0)
					size = force_size;
				//printf("New size %i byte(s)\n", size);
				size = no_gba_savTrim(out_buf, size);
				//printf("--- new size after trim %i byte(s)\n", size);
				size = no_gba_fillLeft(size);
				//printf("--- new size after fill %i byte(s)\n", size);
				raw_applyUserSettings(size, (force_size > 0));
				resize(size);
				for (u32 tt = 0; tt < size; tt++)
					data[tt] = out_buf[tt];

				//dump back out as a dsv, just to keep things sane
				flush();
				printf("---- Loaded no$GBA save\n");

				if (in_buf) delete [] in_buf;
				if (out_buf) delete [] out_buf;
				fclose(fsrc);
				return true;
			}
			if (out_buf) delete [] out_buf;
		}
		if (in_buf) delete [] in_buf;
		fclose(fsrc);
	}

	return false;
}

bool BackupDevice::save_no_gba(const char* fname)
{
	FILE* outf = fopen(fname,"wb");
	if(!outf) return false;
	u32 size = data.size();
	u32 padSize = pad_up_size(size);
	if(data.size()>0)
		fwrite(&data[0],1,size,outf);
	for(u32 i=size;i<padSize;i++)
		fputc(0xFF,outf);

	if (padSize < 512 * 1024)
	{
		for(u32 i=padSize; i<512 * 1024; i++)
			fputc(0xFF,outf);
	}
	fclose(outf);
	return true;
}
//======================================================================= end
//=======================================================================
//======================================================================= no$GBA


void BackupDevice::loadfile()
{
	//never use save files if we are in movie mode
	if(isMovieMode) return;
	if(filename.length() ==0) return; //No sense crashing if no filename supplied

#ifdef _DONT_LOAD_BACKUP
	return;
#endif

	EMUFILE_FILE* inf = new EMUFILE_FILE(filename.c_str(),"rb");
	if(inf->fail())
	{
		delete inf;
		//no dsv found; we need to try auto-importing a file with .sav extension 
		printf("DeSmuME .dsv save file not found. Trying to load an old raw .sav file.\n");
		
		//change extension to sav
		char tmp[MAX_PATH] = {0};
		path.getpathnoext(path.BATTERY, tmp);
		strcat(tmp, ".sav");

		inf = new EMUFILE_FILE(tmp,"rb");
		if(inf->fail())
		{
			delete inf;
			printf("Missing save file %s\n",filename.c_str());
			return;
		}
		delete inf;

		if (!load_no_gba(tmp))
			load_raw(tmp);
	}
	else
	{
		//scan for desmume save footer
		const s32 cookieLen = (s32)strlen(kDesmumeSaveCookie);
		char *sigbuf = new char[cookieLen];
		inf->fseek(-cookieLen, SEEK_END);
		inf->fread(sigbuf,cookieLen);
		int cmp = memcmp(sigbuf,kDesmumeSaveCookie,cookieLen);
		delete[] sigbuf;
		if(cmp)
		{
			//maybe it is a misnamed raw save file. try loading it that way
			printf("Not a DeSmuME .dsv save file. Trying to load as raw.\n");
			delete inf;
			if (!load_no_gba(filename.c_str()))
				load_raw(filename.c_str());
			return;
		}
		//desmume format
		inf->fseek(-cookieLen, SEEK_END);
		inf->fseek(-4, SEEK_CUR);
		u32 version = 0xFFFFFFFF;
		read32le(&version,inf);
		if(version!=0) {
			printf("Unknown save file format\n");
			return;
		}
		inf->fseek(-24, SEEK_CUR);
		read32le(&info.size,inf);
		read32le(&info.padSize,inf);
		read32le(&info.type,inf);
		read32le(&info.addr_size,inf);
		read32le(&info.mem_size,inf);

		u32 left = 0;
		if (CommonSettings.autodetectBackupMethod == 1)
		{
			if (advsc.isLoaded())
			{
				info.type = advsc.getSaveType();
				if (info.type != 0xFF && info.type != 0xFE)
				{
					info.type++;
					u32 adv_size = save_types[info.type].size;
					if (info.size > adv_size)
						info.size = adv_size;
					else
						if (info.size < adv_size)
						{
							left = adv_size - info.size;
							info.size = adv_size;
						}
				}
			}
		}
		//establish the save data
		resize(info.size);
		inf->fseek(0, SEEK_SET);
		if(info.size>0)
			inf->fread(&data[0],info.size - left); //read all the raw data we have
		state = RUNNING;
		addr_size = info.addr_size;
		//none of the other fields are used right now


		if (CommonSettings.autodetectBackupMethod != 1 && info.type == 0) 
		{
			info.type = searchFileSaveType(info.size);
			if (info.type == 0xFF) info.type = 0;
		}

		u32 ss = (info.padSize * 8) / 1024;
		bool _Mbit = false;

		if (ss >= 1024)
		{
			ss /= 1024;
			_Mbit = true;
		}

		printf("Backup size: %u %cbit\n", ss, _Mbit?'M':'K');

		delete inf;
	}
}

bool BackupDevice::save_raw(const char* filename)
{
	FILE* outf = fopen(filename,"wb");
	if(!outf) return false;
	u32 size = data.size();
	u32 padSize = pad_up_size(size);
	if(data.size()>0)
		fwrite(&data[0],1,size,outf);
	for(u32 i=size;i<padSize;i++)
		fputc(kUninitializedSaveDataValue,outf);
	fclose(outf);
	return true;
}

u32 BackupDevice::pad_up_size(u32 startSize)
{
	u32 size = startSize;
	u32 ctr=0;
	while(ctr<saveSizes_count && size > saveSizes[ctr]) ctr++;
	u32 padSize = saveSizes[ctr];
	if(padSize == 0xFFFFFFFF)
	{
		printf("PANIC! Couldn't pad up save size. Refusing to pad.\n");
		padSize = startSize;
	}
		return padSize;
}

void BackupDevice::lazy_flush()
{
	if(flushPending || lazyFlushPending)
	{
		lazyFlushPending = flushPending = false;
		flush();
	}
}

void BackupDevice::flush()
{
	//never use save files if we are in movie mode
	if(isMovieMode) return;

#ifdef _DONT_SAVE_BACKUP
	return;
#endif

	if (filename.length() == 0) return;

	EMUFILE* outf = new EMUFILE_FILE(filename.c_str(),"wb");
	if(!outf->fail())
	{
		if(data.size()>0)
			outf->fwrite(&data[0],data.size());
		
		//write the footer. we use a footer so that we can maximize the chance of the
		//save file being recognized as a raw save file by other emulators etc.
		
		//first, pad up to the next largest known save size.
		u32 size = data.size();
		u32 padSize = pad_up_size(size);

		for(u32 i=size;i<padSize;i++)
			outf->fputc(kUninitializedSaveDataValue);

		//this is just for humans to read
		outf->fprintf("|<--Snip above here to create a raw sav by excluding this DeSmuME savedata footer:");

		//and now the actual footer
		write32le(size,outf); //the size of data that has actually been written
		write32le(padSize,outf); //the size we padded it to
		write32le(info.type,outf); //save memory type
		write32le(addr_size,outf);
		write32le(info.size,outf); //save memory size
		write32le(0,outf); //version number
		outf->fprintf("%s", kDesmumeSaveCookie); //this is what we'll use to recognize the desmume format save

		delete outf;
	}
	else
	{
		delete outf;
		printf("Unable to open savefile %s\n", filename.c_str());
	}
}

void BackupDevice::raw_applyUserSettings(u32& size, bool manual)
{
	//respect the user's choice of backup memory type
	if(CommonSettings.manualBackupType == MC_TYPE_AUTODETECT && !manual)
	{
		addr_size = addr_size_for_old_save_size(size);
		resize(size);
	}
	else
	{
		u32 type = CommonSettings.manualBackupType;
		if (manual)
		{
			u32 res = searchFileSaveType(size);
			if (res != 0xFF) type = (res + 1); // +1 - skip autodetect
		}
		int savetype = save_types[type].media_type;
		int savesize = save_types[type].size;
		addr_size = addr_size_for_old_save_type(savetype);
		if((u32)savesize<size) size = savesize;
		resize(savesize);
	}

	state = RUNNING;
}

u32 BackupDevice::get_save_raw_size(const char* fname)
{
	FILE* inf = fopen(fname,"rb");
	if (!inf) return 0xFFFFFFFF;

	fseek(inf, 0, SEEK_END);
	u32 size = (u32)ftell(inf);
	fclose(inf);
	return size;
}

bool BackupDevice::load_raw(const char* filename, u32 force_size)
{
	FILE* inf = fopen(filename,"rb");

	if (!inf) return false;

	fseek(inf, 0, SEEK_END);
	u32 size = (u32)ftell(inf);
	u32 left = 0;
	
	if (force_size > 0)
	{
		if (size > force_size)
			size = force_size;
		else
			if (size < force_size)
			{
				left = force_size - size;
				size = force_size;
			}
	}

	fseek(inf, 0, SEEK_SET);

	raw_applyUserSettings(size, (force_size > 0));

	fread(&data[0],1,size-left,inf);
	fclose(inf);

	//dump back out as a dsv, just to keep things sane
	flush();

	return true;
}

u32 BackupDevice::get_save_duc_size(const char* fname)
{
	FILE* inf = fopen(fname,"rb");
	if (!inf) return 0xFFFFFFFF;

	fseek(inf, 0, SEEK_END);
	u32 size = (u32)ftell(inf);
	fclose(inf);
	if (size < 500) return 0xFFFFFFFF;
	return (size - 500);
}

bool BackupDevice::load_duc(const char* filename, u32 force_size)
{
  u32 size;
   char id[16];
   FILE* file = fopen(filename, "rb");
   
   if(!file) return false;

   fseek(file, 0, SEEK_END);
   size = (u32)ftell(file) - 500;
   fseek(file, 0, SEEK_SET);

   // Make sure we really have the right file
   fread((void *)id, sizeof(char), 16, file);

   if (memcmp(id, "ARDS000000000001", 16) != 0)
   {
		printf("Not recognized as a valid DUC file\n");
		fclose(file);
		return false;
   }
   // Skip the rest of the header since we don't need it
   fseek(file, 500, SEEK_SET);

	u32 left = 0;
	if (force_size > 0)
	{
		if (size > force_size)
			size = force_size;
		else
			if (size < force_size)
			{
				left = force_size - size;
				size = force_size;
			}
	}

   raw_applyUserSettings(size, (force_size > 0));

   ensure((u32)size);

   fread(&data[0],1,size-left,file);
   fclose(file);

   //choose 

   flush();

   return true;

}

bool BackupDevice::load_movie(EMUFILE* is) {

	const s32 cookieLen = (s32)strlen(kDesmumeSaveCookie);

	is->fseek(-cookieLen, SEEK_END);
	is->fseek(-4, SEEK_CUR);

	u32 version = 0xFFFFFFFF;
	is->fread((char*)&version,4);
	if(version!=0) {
		printf("Unknown save file format\n");
		return false;
	}
	is->fseek(-24, SEEK_CUR);

	struct{
		u32 size,padSize,type,addr_size,mem_size;
	}info;

	is->fread((char*)&info.size,4);
	is->fread((char*)&info.padSize,4);
	is->fread((char*)&info.type,4);
	is->fread((char*)&info.addr_size,4);
	is->fread((char*)&info.mem_size,4);

	//establish the save data
	resize(info.size);
	is->fseek(0, SEEK_SET);
	if(info.size>0)
		is->fread((char*)&data[0],info.size);

	state = RUNNING;
	addr_size = info.addr_size;
	//none of the other fields are used right now

	return true;
}

void BackupDevice::forceManualBackupType()
{
	addr_size = addr_size_for_old_save_size(save_types[CommonSettings.manualBackupType].size);
	state = RUNNING;
}
