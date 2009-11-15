/*  Copyright (C) 2009 CrazyMax
	Copyright (C) 2009 DeSmuME team

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
#include "../mem.h"
#include <string.h>
#include "../MMU.h"

//SRAM is going to be stored just above the rom.
//that is convenient for us, since it mirrors the nds memory map
#define GBA_ROMSIZE (32 * 1024 * 1024) + 1
#define GBA_SAVESIZE (512 * 1024) + 1

static u8		*GBArom = NULL;
static u8		*saveData = NULL;
static u8		saveType = 0xFF;

//================================================================================== Flash GBA
typedef struct 
{
	u8	state;
	u8	cmd;
	u32	size;
	u8	idDevice;
	u8	idManufacturer;
	u8	bank;
} FLASH_GBA;

FLASH_GBA	gbaFlash = {0};

static void gbaWriteFlash(u32 adr, u8 val)
{
	switch (gbaFlash.state)
	{
		case 0:
			if (adr == 0x0A005555)
			{
				if (val == 0xF0)
				{
					//INFO("GBAgame: Flash: reset\n");
					gbaFlash.state = 0;
					gbaFlash.cmd = 0;
					return;
				}
				if (val == 0xAA)
				{
					gbaFlash.state = 1;
					return;
				}
			}

			if (adr == 0x0A000000)
			{
				if (gbaFlash.cmd == 0xB0)
				{
					gbaFlash.bank = val;
					gbaFlash.cmd = 0;
					//INFO("GBAgame: Flash: change bank %i\n", val);
					return;
				}
			}
			break;
		case 1:
			if ( (adr == 0x0A002AAA) && (val == 0x55) )
			{
				gbaFlash.state = 2;
				return;
			}
			gbaFlash.state = 0;
			break;
		case 2:
			if (adr == 0x0A005555)
			{
				//INFO("GBAgame: Flash: send command flash 0x%02X\n", val);
				switch (val)
				{
					case 0x80:		// Erase
						gbaFlash.state = 0x80;
					break;

					case 0x90:		// Chip Identification
						gbaFlash.state = 0x90;
					break;

					case 0xA0:		// Write
						gbaFlash.state = 0;
					break;

					default:
						gbaFlash.state = 0;
					break;
				}
				gbaFlash.cmd = val;
				return;
			}
			gbaFlash.state = 0;
		break;

		// erase
		case 0x80:
			if ( (adr == 0x0A005555) && (val == 0xAA) )
			{
				gbaFlash.state = 0x81;
				return;
			}
			gbaFlash.state = 0;
		break;

		case 0x81:
			if ( (adr == 0x0A002AAA) && (val == 0x55) )
			{
				gbaFlash.state = 0x82;
				return;
			}
			gbaFlash.state = 0;
		break;

		case 0x82:
			if (val == 0x30)
			{
				u32 ofs = (adr & 0x0000F000);
				//INFO("GBAgame: Flash: erase from 0x%08X to 0x%08X\n", ofs + 0x0A000000, ofs + 0x0A001000);
				for (u32 i = ofs; i < (ofs + 0x1000); i++)
					saveData[i] = 0xFF;
			}
			gbaFlash.state = 0;
			gbaFlash.cmd = 0;
		return;

		// Chip Identification
		case 0x90:
			if ( (adr == 0x0A005555) && (val == 0xAA) )
			{
				gbaFlash.state = 0x91;
				return;
			}
			gbaFlash.state = 0;
		break;

		case 0x91:
			if ( (adr == 0x0A002AAA) && (val == 0x55) )
			{
				gbaFlash.state = 0x92;
				return;
			}
			gbaFlash.state = 0;
		break;

		case 0x92:
			gbaFlash.state = 0;
			gbaFlash.cmd = 0;
		return;
	}

	if (gbaFlash.cmd == 0xA0)	// write
	{
		saveData[(adr & 0x1FFFF)+(0x10000*gbaFlash.bank)] = val;
		gbaFlash.state = 0;
		gbaFlash.cmd = 0;
		return;
	}
	INFO("GBAgame: Flash: write unknown atn 0x%08X = 0x%02X\n", adr, val);
}

static u8 gbaReadFlash(u32 adr)
{
	if (gbaFlash.cmd == 0)
	{
		//INFO("GBAgame: flash read at 0x%08X = 0x%02X\n", adr, saveData[(adr & 0x1FFFF)+(0x10000*gbaFlash.bank)]);
		return saveData[(adr & 0x1FFFF)+(0x10000*gbaFlash.bank)];
	}

	//INFO("GBAgame: flash read at 0x%08X\n", adr);

	switch (gbaFlash.cmd)
	{
		case 0x90:			// Chip Identification
			if (adr == 0x0A000000) return gbaFlash.idManufacturer;
			if (adr == 0x0A000001) return gbaFlash.idDevice;
		break;

		case 0xF0:			//
			//INFO("GBAgame: Flash: reset2\n");
			gbaFlash.state = 0;
			gbaFlash.cmd = 0;
		break;

		case 0xB0:			// Bank switching
		break;

		default:
			INFO("GBAgame: Flash: read - unknown command at 0x%08X = 0x%02X\n", adr, gbaFlash.cmd);
		break;
	}

	return 0xFF;
}
//==================================================================================

static u8 getSaveTypeGBA(const u8 *data, const u32 size)
{
	u8	type = 0;
	u8	*dat = (u8 *)data;
	
	for (u32 i = 0; i < (size / 4); i++)
	{
		u32	tmp = T1ReadLong(dat, i);

		if (tmp == 0x52504545)
		{
			if(memcmp(dat, "EEPROM_", 7) == 0)
			{
				return 1;
			}
		}

		if (tmp == 0x4D415253)
		{
			if(memcmp(dat, "SRAM_", 5) == 0)
			{
				return 2;
			}
		}

		if (tmp == 0x53414C46)
		{
			if(memcmp(dat, "FLASH1M_", 8) == 0)
			{
				return 3;
			}
		}

		if (tmp == 0x52494953)
		{
			if(memcmp(dat, "SIIRTC_V", 8) == 0)
			{
				return 4;
			}
		}

		if(memcmp(dat, "FLASH", 5) == 0)
		{
			return 5;
		}
		dat += 4;
	}

	return 0xFF;		// NONE
}

static BOOL GBAgame_init(void)
{
	return (TRUE); 
}

static void GBAgame_reset(void)
{
	memset(&gbaFlash, 0, sizeof(gbaFlash));

	if (GBArom)
	{
		delete [] GBArom;
		GBArom = NULL;
	}
	GBArom = new u8 [GBA_ROMSIZE];
	memset(GBArom, 0xFF, GBA_ROMSIZE);

	if (saveData)
	{
		delete [] saveData;
		saveData = NULL;
	}
	saveData = new u8 [GBA_SAVESIZE];
	memset(saveData, 0xFF, GBA_SAVESIZE);

	if (!strlen(GBAgameName)) return;
	FILE *fgame = 0;

	fgame = fopen(GBAgameName,"rb");
	if (!fgame) return;
	fseek(fgame, 0, SEEK_END);
	u32 size = ftell(fgame);
	rewind(fgame);

	if (!fread(GBArom, 1, size, fgame))
	{
		fclose(fgame);
		return;
	}
	fclose(fgame);

	saveType = getSaveTypeGBA(GBArom, size);
	INFO("Loaded \"%s\" in GBA slot (save type %i)\n", GBAgameName, saveType);

	//try loading the sram
	char * dot = strrchr(GBAgameName,'.');
	if(!dot) return;
	std::string sram_fname = GBAgameName;
	sram_fname.resize(dot-GBAgameName);
	sram_fname += ".sav";
	fgame = fopen(sram_fname.c_str(),"rb");
	if(!fgame) return;

	fseek(fgame, 0, SEEK_END);
	size = ftell(fgame);
	rewind(fgame);

	if (!fread(saveData, 1, size, fgame))
	{
		fclose(fgame);
		return;
	}
	fclose(fgame);
	
	gbaFlash.size = size;
	if (gbaFlash.size <= (64 * 1024))
	{
		gbaFlash.idDevice = 0x1B;
		gbaFlash.idManufacturer = 0x32;
	}
	else
	{
		gbaFlash.idDevice = 0x09;
		gbaFlash.idManufacturer = 0xC2;
	}
	
	INFO("Loaded save \"%s\" in GBA slot\n", sram_fname.c_str());
}

static void GBAgame_close(void)
{
	if (GBArom)
	{
		delete [] GBArom;
		GBArom = NULL;
	}

	if (saveData)
	{
		delete [] saveData;
		saveData = NULL;
	}
}

static void GBAgame_config(void) {}

static void GBAgame_write08(u32 adr, u8 val)
{
	//INFO("GBAgame: write08 at 0x%08X val=0x%02X\n", adr, val);
	if ( (adr >= 0x0A000000) && (adr < 0x0A010000) )
	{
		switch (saveType)
		{
			case 3:			// Flash
			case 5:
				gbaWriteFlash(adr, val);
			break;

			default:
				break;
		}
		//return (u8)T1ReadByte(saveData, (adr - 0x0A000000));
	}
}
static void GBAgame_write16(u32 adr, u16 val)
{
	//INFO("GBAgame: write16 at 0x%08X val=0x%04X\n", adr, val); 
}

static void GBAgame_write32(u32 adr, u32 val)
{
	//INFO("GBAgame: write32 at 0x%08X val=0x%08X\n", adr, val);
}

static u8   GBAgame_read08(u32 adr)
{ 
	//INFO("GBAgame: read08 at 0x%08X value 0x%02X\n", adr, (u8)T1ReadByte(GBArom, (adr - 0x08000000)));
	
	if (adr < 0x0A000000)
		return (u8)T1ReadByte(GBArom, (adr - 0x08000000));

	if (adr < 0x0A010000)
	{
		switch (saveType)
		{
			case 3:			// Flash
			case 5:
				return gbaReadFlash(adr);

			default:
				break;
		}
		
		//INFO("Read08 at 0x%08X val=0x%08X\n", adr, (u8)T1ReadByte(GBArom, (adr - 0x08000000)) );
		return (u8)T1ReadByte(saveData, (adr - 0x0A000000));
	}

	//INFO("Read08 at 0x%08X val=0x%08X\n", adr, (u8)T1ReadByte(GBArom, (adr - 0x08000000)) );

	return 0xFF;
}

static u16  GBAgame_read16(u32 adr)
{ 
	//INFO("GBAgame: read16 at 0x%08X value 0x%04X\n", adr, (u16)T1ReadWord(GBArom, (adr - 0x08000000)));
	
	if (adr < 0x0A000000)
		return (u16)T1ReadWord(GBArom, (adr - 0x08000000));

	if (adr < 0x0A010000)
	{
		//INFO("GBAgame: flash read16 at 0x%08X\n", adr);
		return (u16)T1ReadWord(saveData, (adr - 0x0A000000));
	}
	return 0xFFFF;
}

static u32  GBAgame_read32(u32 adr)
{ 
	//INFO("GBAgame: read32 at 0x%08X value 0x%08X\n", adr, (u32)T1ReadLong(GBArom, (adr - 0x08000000)));

	if (adr < 0x0A000000)
		return (u32)T1ReadLong(GBArom, (adr - 0x08000000));

	if (adr < 0x0A010000)
	{
		//INFO("GBAgame: flash read32 at 0x%08X\n", adr);
		return (u32)T1ReadLong(saveData, (adr - 0x0A000000));
	}
	return 0xFFFFFFFF;
}

static void GBAgame_info(char *info)
{
	strcpy(info, "GBA game in slot");
}

ADDONINTERFACE addonGBAgame = {
				"GBA game",
				GBAgame_init,
				GBAgame_reset,
				GBAgame_close,
				GBAgame_config,
				GBAgame_write08,
				GBAgame_write16,
				GBAgame_write32,
				GBAgame_read08,
				GBAgame_read16,
				GBAgame_read32,
				GBAgame_info};
