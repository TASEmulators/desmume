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

#include "slot1comp_rom.h"
#include "MMU.h"
#include "NDSSystem.h"


void Slot1Comp_Rom::start(eSlot1Operation operation, u32 addr)
{
	this->operation = operation;
	this->address = addr;
}

u32 Slot1Comp_Rom::read()
{
	switch(operation)
	{
	case eSlot1Operation_00_ReadHeader_Unencrypted:
		{
			u32 ret = gameInfo.readROM(address);
			address = (address + 4) & 0xFFF;
			return ret;
		}
		break;

	case eSlot1Operation_2x_SecureAreaLoad:
		{
			//see B7 for details
			address &= gameInfo.mask; //sanity check
			u32 ret = gameInfo.readROM(address);
			address = (address&~0xFFF) + ((address+4)&0xFFF);
			return ret;
		}

	case eSlot1Operation_B7_Read:
		{
			//TODO - check about non-4-byte aligned addresses

			//OBSOLETED?
			////it seems that etrian odyssey 3 doesnt work unless we mask this to cart size.
			////but, a thought: does the internal rom address counter register wrap around? we may be making a mistake by keeping the extra precision
			////but there is no test case yet
			//at any rate, this is good for safety's sake.

			address &= gameInfo.mask;

			//"Can be used only for addresses 8000h and up, smaller addresses will be silently redirected to address `8000h+(addr AND 1FFh)`"
			if(address < 0x8000)
				address = (0x8000 + (address & 0x1FF));

			//as a sanity measure for funny-sized roms (homebrew and perhaps truncated retail roms)
			//we need to protect ourselves by returning 0xFF for things still out of range
			if (address > gameInfo.header.endROMoffset)
			{
				DEBUG_Notify.ReadBeyondEndOfCart(address,gameInfo.romsize);
				return 0xFFFFFFFF;
			}

			//actually read from the ROM provider
			u32 ret = gameInfo.readROM(address);

			//"However, the datastream wraps to the begin of the current 4K block when address+length crosses a 4K boundary (1000h bytes)"
			address = (address&~0xFFF) + ((address+4)&0xFFF);

			return ret;
		}
		break;

	default:
		return 0;

	} //switch(operation)
} //Slot1Comp_Rom::read()

u32 Slot1Comp_Rom::getAddress()
{
	return address & gameInfo.mask;
} //Slot1Comp_Rom::getAddress()

u32 Slot1Comp_Rom::incAddress()
{
	address &= gameInfo.mask;
	address = (address&~0xFFF) + ((address+4)&0xFFF);
	return address;
}


void Slot1Comp_Rom::savestate(EMUFILE* os)
{
	s32 version = 0;
	os->write32le(version);
	os->write32le((s32)operation);
	os->write32le(address);
}

void Slot1Comp_Rom::loadstate(EMUFILE* is)
{
	s32 version = is->read32le();
	operation = (eSlot1Operation)is->read32le();
	address = is->read32le();
}