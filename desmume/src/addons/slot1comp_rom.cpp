/*
	Copyright (C) 2010-2021 DeSmuME team

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

#include "../NDSSystem.h"
#include "../emufile.h"


void Slot1Comp_Rom::start(eSlot1Operation operation, u32 addr)
{
	this->_operation = operation;
	this->_address = addr;
}

u32 Slot1Comp_Rom::read()
{
	switch(this->_operation)
	{
	case eSlot1Operation_00_ReadHeader_Unencrypted:
		{
			u32 ret = gameInfo.readROM(this->_address);
			this->_address = (this->_address + 4) & 0xFFF;
			return ret;
		}
		break;

	case eSlot1Operation_2x_SecureAreaLoad:
		{
			//see B7 for details

			//zero 15-sep-2014 - this is meaningless. newer mask is actually reasonable
			//address &= gameInfo.mask; //sanity check 
			u32 secureAreaAddress = (this->_address - 0x4000);
			secureAreaAddress &= 0x3FFF; //memory safe sanity test
			u32 ret = LE_TO_LOCAL_32(*(u32*)(gameInfo.secureArea + secureAreaAddress));
			this->_address = (this->_address&~0xFFF) + ((this->_address+4)&0xFFF);
			return ret;
		}

	case eSlot1Operation_B7_Read:
		{
			//TODO - check about non-4-byte aligned addresses

			//it seems that etrian odyssey 3 doesnt work unless we mask this to cart size.
			//but, a thought: does the internal rom address counter register wrap around? we may be making a mistake by keeping the extra precision
			//but there is no test case yet
			this->_address &= gameInfo.mask;

			//feature of retail carts: 
			//B7 "Can be used only for addresses 8000h and up, smaller addresses will be silently redirected to address `8000h+(addr AND 1FFh)`"
			if(CommonSettings.RetailCardProtection8000)
				if(this->_address < 0x8000)
					this->_address = (0x8000 + (this->_address & 0x1FF));

			//1. as a sanity measure for funny-sized roms (homebrew and perhaps truncated retail roms) we need to protect ourselves by returning 0xFF for things still out of range.
			//2. this isnt right, unless someone documents otherwise:
			//if (address > gameInfo.header.endROMoffset)
			//  ... the cart hardware doesnt know anything about the rom header. if it has a totally bogus endROMoffset, the cart will probably work just fine. and, the +4 is missing anyway:
			//3. this is better: it just allows us to read 0xFF anywhere we dont have rom data. forget what the header says
			//note: we allow the reading to proceed anyway, because the readROM method is built to allow jaggedy reads off the end of the rom to support trimmed roms
			if(this->_address+4 > gameInfo.romsize)
			{
				DEBUG_Notify.ReadBeyondEndOfCart(this->_address,gameInfo.romsize);
			}

			//actually read from the ROM provider
			u32 ret = gameInfo.readROM(this->_address);

			//"However, the datastream wraps to the begin of the current 4K block when address+length crosses a 4K boundary (1000h bytes)"
			this->_address = (this->_address&~0xFFF) + ((this->_address+4)&0xFFF);

			return ret;
		}
		break;

	default:
		return 0;

	} //switch(operation)
} //Slot1Comp_Rom::read()

u32 Slot1Comp_Rom::getAddress()
{
	return this->_address & gameInfo.mask;
} //Slot1Comp_Rom::getAddress()

u32 Slot1Comp_Rom::incAddress()
{
	this->_address &= gameInfo.mask;
	this->_address = (this->_address&~0xFFF) + ((this->_address+4)&0xFFF);
	return this->_address;
}


void Slot1Comp_Rom::savestate(EMUFILE &os)
{
	s32 version = 0;
	os.write_32LE(version);
	os.write_32LE((s32)this->_operation);
	os.write_32LE(this->_address);
}

void Slot1Comp_Rom::loadstate(EMUFILE &is)
{
	s32 version = is.read_s32LE();
	this->_operation = (eSlot1Operation)is.read_s32LE();
	this->_address = is.read_u32LE();
}
