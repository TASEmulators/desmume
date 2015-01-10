/*
	Copyright (C) 2013-2015 DeSmuME team

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

#include "slot1comp_mc.h"

#include "../MMU.h"
#include "../NDSSystem.h"

Slot1Comp_MC g_Slot1Comp_MC;

u8 Slot1Comp_MC::auxspi_transaction(int PROCNUM, u8 value)
{
	return MMU_new.backupDevice.data_command(value, PROCNUM);
}

void Slot1Comp_MC::auxspi_reset(int PROCNUM)
{
	MMU_new.backupDevice.reset_command();
}

void Slot1Comp_MC::connect()
{
	if(!memcmp(gameInfo.header.gameCode,"AXBJ", 4)) MMU_new.backupDevice.uninitializedValue = 0x00; // Daigassou! Band Brothers DX (JP)
}