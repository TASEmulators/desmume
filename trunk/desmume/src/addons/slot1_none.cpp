/*
	Copyright (C) 2010-2013 DeSmuME team

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

#include "../slot1.h"
#include "../registers.h"
#include "../MMU.h"

static void slot1_info(char *info) { strcpy(info, "Slot1 no-card emulation"); }
static void slot1_config(void) {}

static BOOL slot1_init() { return (TRUE); }

static void slot1_reset()
{
	// Write the header checksum to memory (the firmware needs it to see the cart)
	_MMU_write16<ARMCPU_ARM9>(0x027FF808, 0);
}

static void slot1_close() {}


static void slot1_write08(u8 PROCNUM, u32 adr, u8 val) {}
static void slot1_write16(u8 PROCNUM, u32 adr, u16 val) {}
static void slot1_write32(u8 PROCNUM, u32 adr, u32 val) {}

static u8 slot1_read08(u8 PROCNUM, u32 adr)
{
	return 0xFF;
}
static u16 slot1_read16(u8 PROCNUM, u32 adr)
{
	return 0xFFFF;
}
static u32 slot1_read32(u8 PROCNUM, u32 adr)
{
	if (adr == REG_GCDATAIN && MMU.dscard[PROCNUM].command[0] == 0xB8)
		return 0;
	return 0xFFFFFFFF;
}


SLOT1INTERFACE slot1None = {
	"None",
	slot1_init,
	slot1_reset,
	slot1_close,
	slot1_config,
	slot1_write08,
	slot1_write16,
	slot1_write32,
	slot1_read08,
	slot1_read16,
	slot1_read32,
	slot1_info};
