/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright (C) 2009 CrazyMax
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

static u8		*GBArom = NULL;

#define GBA_ROMSIZE (32 * 1024 * 1024)
#define GBA_RAMSIZE (64 * 1024)
#define GBA_SIZE (GBA_ROMSIZE+GBA_RAMSIZE)

static BOOL GBAgame_init(void)
{
	GBArom = new u8 [GBA_SIZE];
	return (TRUE); 
}

static void GBAgame_reset(void)
{
	memset(GBArom, 0, GBA_SIZE);

	if (!strlen(GBAgameName)) return;
	FILE *fgame = 0;

	fgame = fopen(GBAgameName,"rb");
	if (!fgame) return;
	INFO("Loaded \"%s\" in GBA slot\n", GBAgameName);
	fseek(fgame, 0, SEEK_END);
	u32 size = ftell(fgame);
	rewind(fgame);

	if (!fread(GBArom, 1, size, fgame))
	{
		fclose(fgame);
		return;
	}

	fclose(fgame);

	//try loading the sram
	char * dot = strrchr(GBAgameName,'.');
	if(!dot) return;
	std::string sram_fname = GBAgameName;
	sram_fname.resize(dot-GBAgameName);
	sram_fname += ".sav";
	fgame = fopen(sram_fname.c_str(),"rb");
	if(fgame)
	{
		fread(GBArom+GBA_ROMSIZE,1,GBA_RAMSIZE,fgame);
		fclose(fgame);
	}
}

static void GBAgame_close(void)
{
	delete[] GBArom;
	GBArom = NULL;
}

static void GBAgame_config(void) {}
static void GBAgame_write08(u32 adr, u8 val){}
static void GBAgame_write16(u32 adr, u16 val) {}
static void GBAgame_write32(u32 adr, u32 val) {}

static u8   GBAgame_read08(u32 adr)
{ 
	//INFO("Read08 at 0x%08X value 0x%02X\n", adr, (u8)T1ReadByte(GBArom, (adr - 0x08000000)));
	if ( (adr >= 0x08000004) && (adr < 0x080000A0) )
		return MMU.MMU_MEM[0][0xFF][(adr +0x1C) & MMU.MMU_MASK[0][0xFF]];
	return (u8)T1ReadByte(GBArom, (adr - 0x08000000));
}

static u16  GBAgame_read16(u32 adr)
{ 
	//INFO("Read16 at 0x%08X value 0x%04X\n", adr, (u16)T1ReadWord(GBArom, (adr - 0x08000000)));
	if ( (adr >= 0x08000004) && (adr < 0x080000A0) )
		return T1ReadWord(MMU.MMU_MEM[0][0xFF], (adr +0x1C) & MMU.MMU_MASK[0][0xFF]);  
	return (u16)T1ReadWord(GBArom, (adr - 0x08000000));
}

static u32  GBAgame_read32(u32 adr)
{ 
	//INFO("Read32 at 0x%08X value 0x%08X\n", adr, (u32)T1ReadLong(GBArom, (adr - 0x08000000)));
	if ( (adr >= 0x08000004) && (adr < 0x080000A0) )
		return T1ReadLong(MMU.MMU_MEM[0][0xFF], (adr +0x1C) & MMU.MMU_MASK[0][0xFF]);
	return (u32)T1ReadLong(GBArom, (adr - 0x08000000));
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
