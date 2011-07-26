/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006 Mic
	Copyright (C) 2009-2011 DeSmuME team

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

#include "../addons.h"
#include <string>
#include <string.h>
#include "debug.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include "../utils/vfat.h"
#include "../path.h"

#include "MMU.h"
#include "NDSSystem.h"

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

static BOOL init(void)
{
	return TRUE;
}

static void reset(void)
{
	cflash_close();
	cflash_init();
}

static void close(void)
{
	cflash_close();
}

static void config(void)
{
}

static void write08(u32 procnum, u32 adr, u8 val)
{
	cflash_write(adr, val);
}

static void write16(u32 procnum, u32 adr, u16 val)
{
	cflash_write(adr, val);
}

static void write32(u32 procnum, u32 adr, u32 val)
{
	cflash_write(adr, val);
}

static u8 read08(u32 procnum, u32 adr)
{
	return (cflash_read(adr));
}

static u16 read16(u32 procnum, u32 adr)
{
	return (cflash_read(adr));
}

static u32 read32(u32 procnum, u32 adr)
{
	return (cflash_read(adr));
}

static void info(char *info)
{
	strcpy(info, "MPCF Flash Card Device");
}

ADDONINTERFACE addonCFlash = {
				"MPCF Flash Card Device",
				init,
				reset,
				close,
				config,
				write08,
				write16,
				write32,
				read08,
				read16,
				read32,
				info};

#undef CFLASHDEBUG
