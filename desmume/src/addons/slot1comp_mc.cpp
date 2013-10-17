/*
	Copyright (C) 2013 DeSmuME team

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

//this file contains the components used for emulating standard gamecard "MC" devices (eeprom, fram, flash)
//this is largely done by accessing the BackupDevice resources in the core emulator

#include "types.h"
#include "../MMU.h"
#include "slot1comp_mc.h"

Slot1Comp_MC g_Slot1Comp_MC;

void Slot1Comp_MC::auxspi_reset(const u8 PROCNUM)
{
	MMU_new.backupDevice.reset_command();
}

void Slot1Comp_MC::auxspi_write(const u8 PROCNUM, const u8 size, const u8 adr, u16 cnt)
{
	if (size == 8)
	{
		const u8 ofs = (adr << 3);
		u8 oldCnt = T1ReadByte((u8*)&MMU.AUX_SPI_CNT, (1 - adr));
		cnt = (oldCnt << (8 - ofs)) | (cnt << ofs);
	}

	MMU.AUX_SPI_CNT = cnt;

	//bool enabled = (cnt & (1 << 15))?true:false;
	//bool irq = (cnt & (1 << 14))?true:false;
	//bool spi = (cnt & (1 << 13))?true:false;
	//bool cs = (cnt & (1 << 6))?true:false;

	//printf("MMU%c: write%02d%s to AUX cnt: %08X, CS:%d - %s%s\n", PROCNUM?'7':'9', size, (adr?"+1":""), cnt, cs, spi?"Backup":"NDS Slot", (cnt & (1 << 7))?" - BUSY":"");

	//if (!enabled && irq && !(cnt & 0x3) || spi)
	if (!cnt)
	{
		//printf("MMU%c: reset command (cnt %04x)\n", PROCNUM?'7':'9', cnt);
		auxspi_reset(PROCNUM);
	}
}

u16 Slot1Comp_MC::auxspi_read(const u8 PROCNUM, const u8 size, const u8 adr)
{
	u16 cnt = (MMU.AUX_SPI_CNT >> (adr << 3));

	//bool cs = (cnt & (1 << 6))?true:false;
	//bool spi = (cnt & (1 << 13))?true:false;
	//printf("MMU%c: read%02d%s from AUX cnt: %08X, CS:%d - %s\n", PROCNUM?'7':'9', size, (adr?"+1":""), cnt, cs, spi?"Backup":"NDS Slot");

	return cnt;
}

u8 Slot1Comp_MC::auxspi_transaction(const u8 PROCNUM, u8 value)
{
	u16 cnt = MMU.AUX_SPI_CNT;
	bool spi = (cnt & (1 << 13))?true:false;
	bool cs = (cnt & (1 << 6))?true:false;

	if (spi)
	{
		value = MMU_new.backupDevice.data_command(value, PROCNUM);
		MMU.AUX_SPI_CNT &= ~0x80; //remove busy flag
	}

	return value;
}
