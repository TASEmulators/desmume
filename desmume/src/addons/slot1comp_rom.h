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

//this file contains the components used for emulating standard gamecard ROMs
//this is largely done by accessing the rom provided in the core emulator

#include "slot1comp_protocol.h"


class Slot1Comp_Rom
{
public:
	void start(eSlot1Operation operation, u32 addr);
	u32 read();
	u32 getAddress();
	u32 incAddress();
private:
	u32 address;
	eSlot1Operation operation;
};

