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

#ifndef _SLOT1COMP_MC_H
#define _SLOT1COMP_MC_H

#include "../types.h"

class Slot1Comp_MC
{
public:
	u8 auxspi_transaction(int PROCNUM, u8 value);
	void auxspi_reset(int PROCNUM);
	void connect();
};

extern Slot1Comp_MC g_Slot1Comp_MC;

#endif
