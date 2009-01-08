/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "../addons.h"
#include "../mem.h"
#include "../MMU.h"

u8	rumble_hdr[0xC0];

BOOL RumblePak_init(void) { return (TRUE); };
void RumblePak_reset(void)
{
	memset(rumble_hdr, 0, sizeof(rumble_hdr));
	memcpy(rumble_hdr + 0x04, gba_header_data_0x04, 156);
	memcpy(rumble_hdr + 0xA0, "Rumble Pak  ", 12);
	rumble_hdr[0xB2] = 0x96;
	rumble_hdr[0xBE] = 0x45;
};
void RumblePak_close(void) {};
void RumblePak_config(void) {};
void RumblePak_write08(u32 adr, u8 val)
{
	//INFO("write08 0x%X\n", val);
};
void RumblePak_write16(u32 adr, u16 val)
{
	//INFO("write16 0x%X\n", val);
};
void RumblePak_write32(u32 adr, u32 val)
{
	//INFO("write32 0x%X\n", val);
};
u8   RumblePak_read08(u32 adr)
{
	u8	val = 0;

	if (adr >= 0x08000000 && adr < 0x080000C0)
		val = rumble_hdr[adr & 0xFF];
	if (adr == 0x0801FFFE) val = 0x45;

	return ((u8)val);
};

u16  RumblePak_read16(u32 adr)
{
	u16 val = 0;

	if (adr >= 0x08000000 && adr < 0x080000C0)
		val = T1ReadWord(rumble_hdr, adr & 0xFF );
	if (adr == 0x0801FFFE) val = 0x0045;

	return ((u16)val); 
};
u32  RumblePak_read32(u32 adr)
{
	u32 val = 0;
	if (adr >= 0x08000000 && adr < 0x080000C0)
		val = T1ReadLong(rumble_hdr, adr & 0xFF );
	if (adr == 0x0801FFFE) val = 0x00000045;

	return ((u32)val);
};
void RumblePak_info(char *info) { strcpy(info, "NDS Rumble Pak (need joystick)"); };

ADDONINTERFACE addonRumblePak = {
				"Rumble Pak",
				RumblePak_init,
				RumblePak_reset,
				RumblePak_close,
				RumblePak_config,
				RumblePak_write08,
				RumblePak_write16,
				RumblePak_write32,
				RumblePak_read08,
				RumblePak_read16,
				RumblePak_read32,
				RumblePak_info};
