/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006 Mic
	Copyright (C) 2009-2013 DeSmuME team

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

#include <string>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "../types.h"
#include "../debug.h"
#include "../path.h"
#include "../utils/vfat.h"
#include "../slot2.h"

// Set up addresses for GBAMP
#define CF_REG_DATA 0x9000000
#define CF_REG_ERR 0x9020000
#define CF_REG_SEC 0x9040000
#define CF_REG_LBA1 0x9060000
#define CF_REG_LBA2 0x9080000
#define CF_REG_LBA3 0x90A0000
#define CF_REG_LBA4 0x90C0000
#define CF_REG_CMD 0x90E0000
#define CF_REG_STS 0x98C0000

// CF Card commands
#define CF_CMD_LBA 0xE0
#define CF_CMD_READ 0x20
#define CF_CMD_WRITE 0x30

static u16	cf_reg_sts, 
			cf_reg_lba1,
			cf_reg_lba2,
			cf_reg_lba3,
			cf_reg_lba4,
			cf_reg_cmd;
static off_t currLBA;

static const int lfnPos[13] = {1,3,5,7,9,14,16,18,20,22,24,28,30};

static u32 fileStartLBA,fileEndLBA;
static std::string sFlashPath;
static BOOL cflashDeviceEnabled = FALSE;

static EMUFILE* file = NULL;

// ===========================
BOOL	inited;

enum EListCallbackArg {
	EListCallbackArg_Item, EListCallbackArg_Pop
};

static BOOL cflash_init() 
{
	if (inited) return FALSE;
	BOOL init_good = FALSE;

	CFLASHLOG("CFlash_Mode: %d\n",CFlash_Mode);

	if (CFlash_Mode == ADDON_CFLASH_MODE_RomPath)
	{
		sFlashPath = path.RomDirectory;
		INFO("Using CFlash directory of rom: %s\n", sFlashPath.c_str());
	}
	else if(CFlash_Mode == ADDON_CFLASH_MODE_Path)
	{
		sFlashPath = CFlash_Path;
		INFO("Using CFlash directory: %s\n", sFlashPath.c_str());
	}

	if (sFlashPath == "") return FALSE;

	if(CFlash_IsUsingPath())
	{
		cflashDeviceEnabled = FALSE;
		currLBA = 0;

		fileStartLBA = fileEndLBA = 0xFFFFFFFF;
		VFAT vfat;
		bool ret = vfat.build(sFlashPath.c_str(),16); //allocate 16MB extra for writing. this is probably enough for anyone, but maybe it should be configurable.
		//we could always suggest to users to add a big file to their directory to overwrite (that would cause the image to get padded)

		if(!ret)
		{
			CFLASHLOG("FAILED cflash_build_fat\n");
			return FALSE;
		}

		file = vfat.detach();

		cf_reg_sts = 0x58;	// READY

		cflashDeviceEnabled = TRUE;
		init_good = TRUE;
	}
	else
	{
		sFlashPath = CFlash_Path;
		INFO("Using CFlash disk image file %s\n", sFlashPath.c_str());
		file = new EMUFILE_FILE(sFlashPath.c_str(),"rb+");
		if(file->fail())
		{
			INFO("Failed to open file %s\n", sFlashPath.c_str());
			delete file;
			file = NULL;
		}
	}

	// READY
	cf_reg_sts = 0x58;

	currLBA = 0;
	cf_reg_lba1 = cf_reg_lba2 =
	cf_reg_lba3 = cf_reg_lba4 = 0;

	inited = TRUE;
	return init_good;
}

static unsigned int cflash_read(unsigned int address)
{
	unsigned int ret_value = 0;
	size_t elems_read = 0;

	switch (address)
	{
		case CF_REG_STS:
			ret_value = cf_reg_sts;
		break;

		case CF_REG_DATA:
			if (cf_reg_cmd == CF_CMD_READ)
			{
				if(file)
				{
					u8 data[2];
					file->fseek(currLBA, SEEK_SET);
					elems_read += file->fread(data,2);
					ret_value = data[1] << 8 | data[0];
				}
				currLBA += 2;
			}
		break;

	case CF_REG_CMD:
	break;

	case CF_REG_LBA1:
		ret_value = cf_reg_lba1;
	break;
	}

	return ret_value;
}

static void cflash_write(unsigned int address,unsigned int data)
{
	static u8 sector_data[512];
	static u32 sector_write_index = 0;

	switch (address)
	{
		case CF_REG_STS:
			cf_reg_sts = data&0xFFFF;
		break;

		case CF_REG_DATA:
			if (cf_reg_cmd == CF_CMD_WRITE)
			{
				{
					sector_data[sector_write_index] = (data >> 0) & 0xff;
					sector_data[sector_write_index + 1] = (data >> 8) & 0xff;

					sector_write_index += 2;

					if (sector_write_index == 512) 
					{
						CFLASHLOG( "Write sector to %ld\n", currLBA);
						size_t written = 0;

						if(file) 
							if(currLBA + 512 < file->size()) 
							{
								file->fseek(currLBA,SEEK_SET);
				      
								while(written < 512) 
								{
									size_t todo = 512-written;
									file->fwrite(&sector_data[written], todo);
									size_t cur_write = todo;
									written += cur_write;
									if ( cur_write == (size_t)-1) break;
								}
							}

						CFLASHLOG("Wrote %u bytes\n", written);
					
						currLBA += 512;
						sector_write_index = 0;
					}
				}
			}
		break;

		case CF_REG_CMD:
			cf_reg_cmd = data&0xFF;
			cf_reg_sts = 0x58;	// READY
		break;

		case CF_REG_LBA1:
			cf_reg_lba1 = data&0xFF;
			currLBA = (currLBA&0xFFFFFF00)| cf_reg_lba1;
		break;

		case CF_REG_LBA2:
			cf_reg_lba2 = data&0xFF;
			currLBA = (currLBA&0xFFFF00FF)|(cf_reg_lba2<<8);
		break;

		case CF_REG_LBA3:
			cf_reg_lba3 = data&0xFF;
			currLBA = (currLBA&0xFF00FFFF)|(cf_reg_lba3<<16);
		break;

		case CF_REG_LBA4:
			cf_reg_lba4 = data&0xFF;

			if ((cf_reg_lba4 & 0xf0) == CF_CMD_LBA)
			{
				currLBA = (currLBA&0x00FFFFFF)|((cf_reg_lba4&0x0F)<<24);
				currLBA *= 512;
				sector_write_index = 0;
			}
		break;
	}  
}

static void cflash_close( void) 
{
	if (!inited) return;
  if(file) delete file;
	cflashDeviceEnabled = FALSE;
  file = NULL;
	inited = FALSE;
}

class Slot2_CFlash : public ISlot2Interface
{
public:
	virtual Slot2Info const* info()
	{
		static Slot2InfoSimple info("MPCF Flash Card Device", "MPCF Flash Card Device", 0x01);
		return &info;
	}

	virtual void connect()
	{
		cflash_close();
		cflash_init();
	}
	virtual void disconnect()
	{
		cflash_close();
	}

	virtual void writeByte(u8 PROCNUM, u32 addr, u8 val) { cflash_write(addr, val); }
	virtual void writeWord(u8 PROCNUM, u32 addr, u16 val) { cflash_write(addr, val); }
	virtual void writeLong(u8 PROCNUM, u32 addr, u32 val) { cflash_write(addr, val); }

	virtual u8	readByte(u8 PROCNUM, u32 addr) { return (cflash_read(addr)); }
	virtual u16	readWord(u8 PROCNUM, u32 addr) { return (cflash_read(addr)); }
	virtual u32	readLong(u8 PROCNUM, u32 addr) { return (cflash_read(addr)); }

};

ISlot2Interface* construct_Slot2_CFlash() { return new Slot2_CFlash(); }
