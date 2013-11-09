/*
	Copyright (C) 2013 DeSmuME team

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

static BOOL PassME_init(void) { return (TRUE); }
static void PassME_reset(void) {}
static void PassME_close(void) {}
static void PassME_config(void) {}
static void PassME_write08(u32 procnum, u32 adr, u8 val) {}
static void PassME_write16(u32 procnum, u32 adr, u16 val) {}
static void PassME_write32(u32 procnum, u32 adr, u32 val) {}
static u8   PassME_read08(u32 procnum, u32 adr)
{
	u32 tmp_adr = (adr & 0x07FFFFFF);
	if (tmp_adr < gameInfo.romsize)
		return (u8)gameInfo.readROM(tmp_adr);

	return (0xFF);
}
static u16  PassME_read16(u32 procnum, u32 adr)
{
	u32 tmp_adr = (adr & 0x07FFFFFF);
	if (tmp_adr < gameInfo.romsize)
		return (u16)gameInfo.readROM(tmp_adr);

	return (0xFFFF);
}
static u32  PassME_read32(u32 procnum, u32 adr)
{
	u32 tmp_adr = (adr & 0x07FFFFFF);

	if (tmp_adr < gameInfo.romsize)
		return gameInfo.readROM(tmp_adr);

	return (0xFFFFFFFF);
}
static void PassME_info(char *info) { strcpy(info, "PassME in GBA slot"); }

ADDONINTERFACE addonPassME = {
				"PassME",
				PassME_init,
				PassME_reset,
				PassME_close,
				PassME_config,
				PassME_write08,
				PassME_write16,
				PassME_write32,
				PassME_read08,
				PassME_read16,
				PassME_read32,
				PassME_info};
