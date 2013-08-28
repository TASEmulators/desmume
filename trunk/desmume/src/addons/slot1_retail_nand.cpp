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

// Games with NAND Flash:
// -  Ore/WarioWare D.I.Y.			- 128Mbit
// -  Daigassou! Band Brothers DX	- 64MBit (NAND?)

// Ore/WarioWare D.I.Y. - chip:		SAMSUNG 004
//									KLC2811ANB-P204
//									NTR-UORE-0

#include "../slot1.h"
#include "../registers.h"
#include "../MMU.h"
#include "../NDSSystem.h"
#include "slot1comp_rom.h"
#include "slot1comp_protocol.h"

//quick architecture overview:
//NAND receives GC bus commands from MMU.cpp
//those are passed on to the protocol component for parsing
//protocol calls back into NAND via ISlot1Comp_Protocol_Client interface for things the protocol doesnt know about (the contents of the rom, chiefly)
//NAND utilizes the rom component for address logic and delivering data.
//it also processes some commands itself which arent rom-related (the NANDy stuff)

class Slot1_Retail_NAND : public ISlot1Interface, public ISlot1Comp_Protocol_Client
{
private:
	Slot1Comp_Protocol protocol;
	Slot1Comp_Rom rom;

public:
	virtual Slot1Info const* info()
	{
		static Slot1InfoSimple info("Retail NAND","Slot1 retail NAND card emulation");
		return &info;
	}

	virtual void connect()
	{
		protocol.reset(this);
		protocol.chipId = gameInfo.chipID;
		protocol.gameCode = T1ReadLong((u8*)gameInfo.header.gameCode,0);
	}

	virtual void write_command(u8 PROCNUM, GC_Command command)
	{
		protocol.write_command(command);
	}

	virtual void write_GCDATAIN(u8 PROCNUM, u32 val)
	{
		protocol.write_GCDATAIN(PROCNUM, val);
	}
	virtual u32 read_GCDATAIN(u8 PROCNUM)
	{
		return protocol.read_GCDATAIN(PROCNUM);
	}

	virtual void slot1client_startOperation(eSlot1Operation operation)
	{
		//pass the normal rom operations along to the rom component
		switch(operation)
		{
			case eSlot1Operation_00_ReadHeader_Unencrypted:
			case eSlot1Operation_B7_Read:
				rom.start(operation,protocol.address);
				return;
		}

		//handle special commands ourselves
		int cmd = protocol.command.bytes[0];
		switch(cmd)
		{
			// Nand Init
			case 0x94:
				//GCLOG("NAND 0x94\n");
				//length = 0x200;
				break;

			// Nand Error?
			case 0xD6:
				//GCLOG("NAND 0xD6\n");
				//length = 4;
				break;

			// Nand Write? ---- PROGRAM for INTERNAL DATA MOVE/RANDOM DATA INPUT
			//case 0x8B:
			case 0x85:
				//GCLOG("NAND 0x85\n");
				//length = 0x200;
				break;
		}
	}

	virtual u32 slot1client_read_GCDATAIN(eSlot1Operation operation)
	{
		//pass the normal rom operations along to the rom component
		switch(operation)
		{
			case eSlot1Operation_00_ReadHeader_Unencrypted:
			case eSlot1Operation_B7_Read:
				return rom.read();
		}

		//handle special commands ourselves
		int cmd = protocol.command.bytes[0];
		switch(cmd)
		{
			// Nand Init?
			case 0x94:
				return 0; //Unsure what to return here so return 0 for now

			// Nand Status?
			case 0xD6:
				//0x80 == busy
				// Made in Ore/WarioWare D.I.Y. need set value to 0x80
				return 0x80; //0x20 == ready
		}
		
		return 0;
	}

	virtual void slot1client_write_GCDATAIN(eSlot1Operation operation, u32 val)
	{
		//pass the normal rom operations along to the rom component
		switch(operation)
		{
			case eSlot1Operation_00_ReadHeader_Unencrypted:
			case eSlot1Operation_B7_Read:
				return;
		}

		//handle special commands ourselves
		int cmd = protocol.command.bytes[0];
		switch(cmd)
		{
		}
	}




};

ISlot1Interface* construct_Slot1_Retail_NAND() { return new Slot1_Retail_NAND(); }


