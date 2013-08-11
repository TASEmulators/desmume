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
#include "../NDSSystem.h"

class Slot1_None : public ISlot1Interface
{
public:
	virtual Slot1Info const* info()
	{
		static Slot1InfoSimple info("None","Slot1 no-card emulation");
		return &info;
	}

	virtual void connect()
	{
	}

	virtual u32 read32(u8 PROCNUM, u32 adr)
	{
		//return a chip ID of 0.
		//if (adr == REG_GCDATAIN && MMU.dscard[PROCNUM].command[0] == 0xB8) return 0;
		//EDIT - not sure who did this or why, but... but if a card is ejected, there can be no chip ID.
		//we certainly want it to appear differently from chipId 0, which is something we are faking in other slot-1 devices
		return 0xFFFFFFFF;
	}

};

ISlot1Interface* construct_Slot1_None() { return new Slot1_None(); }