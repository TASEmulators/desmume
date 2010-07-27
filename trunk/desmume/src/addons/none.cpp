/*  Copyright (C) 2009-2010 DeSmuME team

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
#include "../addons.h"
#include "../NDSSystem.h"

static BOOL None_init(void) { return (TRUE); }
static void None_reset(void) {}
static void None_close(void) {}
static void None_config(void) {}
static void None_write08(u32 adr, u8 val) {}
static void None_write16(u32 adr, u16 val) {}
static void None_write32(u32 adr, u32 val) {}
static u8   None_read08(u32 adr)
{
	if (adr > 0x09FFFFFF) 
	{
		if ((adr & 0x0000FFFF) > MMU_new.backupDevice.data.size()) return 0xFF;
		return MMU_new.backupDevice.data[adr & 0x0000FFFF];
	}
	if ((adr - 0x08000000) > gameInfo.romsize) return 0xFF;
	
	return T1ReadByte((u8*)gameInfo.romdata, adr - 0x08000000);
}
static u16  None_read16(u32 adr)
{
	if (adr > 0x09FFFFFF) 
	{
		if ((adr & 0x0000FFFF) > MMU_new.backupDevice.data.size()) return 0xFFFF;
		return (u16)MMU_new.backupDevice.data[adr & 0x0000FFFF];
	}
	if ((adr - 0x08000000) > gameInfo.romsize) return 0xFFFF;
	
	return T1ReadWord((u8*)gameInfo.romdata, adr - 0x08000000);
}
static u32  None_read32(u32 adr)
{
	if (adr > 0x09FFFFFF) 
	{
		if ((adr & 0x0000FFFF) > MMU_new.backupDevice.data.size()) return 0xFFFFFFFF;
		return (u32)MMU_new.backupDevice.data[adr & 0x0000FFFF];
	}

	if ((adr - 0x08000000) > gameInfo.romsize) return 0xFFFFFFFF;
	
	return T1ReadLong((u8*)gameInfo.romdata, adr - 0x08000000);
}
static void None_info(char *info) { strcpy(info, "Nothing in GBA slot"); }

ADDONINTERFACE addonNone = {
				"NONE",
				None_init,
				None_reset,
				None_close,
				None_config,
				None_write08,
				None_write16,
				None_write32,
				None_read08,
				None_read16,
				None_read32,
				None_info};
