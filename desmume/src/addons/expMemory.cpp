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

u8		*expMemory	=	NULL;
u32		expMemSize = 8 * 1024 * 1024; // 8Mb

#if 0
#define EXPINFO(...) INFO(__VA_ARGS__)
#else
#define EXPINFO(...)
#endif 

static BOOL ExpMemory_init(void) { return (TRUE); }
static void ExpMemory_reset(void)
{
	if (expMemory)
	{
		delete [] expMemory;
		expMemory = NULL;
	}
	expMemory = new u8 [expMemSize];
	memset(expMemory, 0xFF, expMemSize);
}
static void ExpMemory_close(void)
{
	if (expMemory)
	{
		delete [] expMemory;
		expMemory = NULL;
	}
}
static void ExpMemory_config(void) {}
static void ExpMemory_write08(u32 adr, u8 val) 
{
	
	if (adr >= 0x09000000)
	{
		u32 offs = (adr - 0x09000000);
		if (offs >= expMemSize) return;
		T1WriteByte(expMemory, offs, val);
	}
	EXPINFO("ExpMemory: write 08 at 0x%08X = 0x%02X\n", adr, val);
}
static void ExpMemory_write16(u32 adr, u16 val) 
{
	
	if (adr >= 0x09000000)
	{
		u32 offs = (adr - 0x09000000);
		if (offs >= expMemSize) return;
		T1WriteWord(expMemory, offs, val);
	}
	EXPINFO("ExpMemory: write 16 at 0x%08X = 0x%04X\n", adr, val);
}
static void ExpMemory_write32(u32 adr, u32 val) 
{
	
	if (adr >= 0x09000000)
	{
		u32 offs = (adr - 0x09000000);
		if (offs >= expMemSize) return;
		T1WriteLong(expMemory, offs, val);
	}
	EXPINFO("ExpMemory: write 32 at 0x%08X = 0x%08X\n", adr, val);
}
static u8   ExpMemory_read08(u32 adr)
{
	if (adr == 0x080000B2) return(0x96);

	if (adr >= 0x09000000)
	{
		u32 offs = (adr - 0x09000000);
		if (offs >= expMemSize) return (0);
		return (T1ReadByte(expMemory, offs));
	}

	EXPINFO("ExpMemory: read 08 at 0x%08X\n", adr);
	return (0);
}
static u16  ExpMemory_read16(u32 adr)
{
	if (adr == 0x080000B6) return(0x2424);
	if (adr == 0x080000BC) return(0x7FFF);
	if (adr == 0x080000BE) return(0x0096);
	if (adr == 0x0801FFFC) return(0x7FFF);

	if (adr >= 0x09000000)
	{
		u32 offs = (adr - 0x09000000);
		if (offs >= expMemSize) return (0);
		return (T1ReadWord(expMemory, offs));
	}

	EXPINFO("ExpMemory: read 16 at 0x%08X\n", adr);
	return (0);
}
static u32  ExpMemory_read32(u32 adr)
{
	if (adr == 0x080000AC) return(0x027FFC30);

	if (adr >= 0x09000000)
	{
		u32 offs = (adr - 0x09000000);
		if (offs >= expMemSize) return (0);
		return (T1ReadLong(expMemory, offs));
	}

	EXPINFO("ExpMemory: read 32 at 0x%08X\n", adr);
	return (0);
}
static void ExpMemory_info(char *info) { strcpy(info, "Memory Expansion Pak"); }

ADDONINTERFACE addonExpMemory = {
				"Memory Expansion Pak",
				ExpMemory_init,
				ExpMemory_reset,
				ExpMemory_close,
				ExpMemory_config,
				ExpMemory_write08,
				ExpMemory_write16,
				ExpMemory_write32,
				ExpMemory_read08,
				ExpMemory_read16,
				ExpMemory_read32,
				ExpMemory_info};
