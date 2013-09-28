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
//protocol calls back into NAND via ISlot1Comp_Protocol_Client interface for things the protocol doesn't know about (the contents of the rom, chiefly)
//NAND utilizes the rom component for address logic and delivering data.
//it also processes some commands itself which aren't rom-related (the NANDy stuff)

class Slot1_Retail_NAND : public ISlot1Interface, public ISlot1Comp_Protocol_Client
{
private:
	u32 subAdr;

	Slot1Comp_Protocol protocol;
	Slot1Comp_Rom rom;

	u32 mode;
	u32 handle_save;
	u32 save_adr;

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

		handle_save = 0;

		subAdr = T1ReadWord(gameInfo.header.unknown5, 0xE) << 17;

		mode = 0;
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
		protocol.address = (protocol.command.bytes[1] << 24) | (protocol.command.bytes[2] << 16) | (protocol.command.bytes[3] << 8) | protocol.command.bytes[4];

		//pass the normal rom operations along to the rom component
		switch(operation)
		{
			case eSlot1Operation_00_ReadHeader_Unencrypted:
			case eSlot1Operation_2x_SecureAreaLoad:
			case eSlot1Operation_B7_Read:
				rom.start(operation,protocol.address);
				return;
		}

		//handle special commands ourselves
		int cmd = protocol.command.bytes[0];
		switch(cmd)
		{
			//Nand Write Page
			case 0x81:
				break;

			// Nand Init
			case 0x94:
				mode = cmd;
				break;

			// Nand Error?
			case 0xD6:
				break;

			case 0x84: //Write disable
			case 0x85: //Write enable
				mode = cmd;
				break;
			case 0x8B:
				mode = cmd;
				handle_save = 0;
				break;

			case 0xB2: //Set save position
				mode = cmd;
				save_adr = protocol.address & gameInfo.mask;
				// to Norrmatt: Made in Ore (UORJ, crc 2E7111B8) crash when save_addr < subAdr
				if (save_adr > subAdr)
					save_adr -= subAdr;
				else
					save_adr = 0;
				handle_save = 1;
				break;
		}
	}

	virtual u32 slot1client_read_GCDATAIN(eSlot1Operation operation)
	{
		//pass the normal rom operations along to the rom component
		switch(operation)
		{
			case eSlot1Operation_00_ReadHeader_Unencrypted:
			case eSlot1Operation_2x_SecureAreaLoad:
			//case eSlot1Operation_B7_Read:
				return rom.read();
		}

		//handle special commands ourselves
		int cmd = protocol.command.bytes[0];
		int val = 0;
		switch(cmd)
		{
			// Nand Init?
			case 0x94:
				val = 0;
				mode = 0;
				break;

			//Rom/Save Read
			case 0xB7:
				{
					if(handle_save)
					{
						MMU_new.backupDevice.ensure(save_adr+4,0);
						val = MMU_new.backupDevice.data[save_adr+3]<<24 | MMU_new.backupDevice.data[save_adr+2]<<16 | MMU_new.backupDevice.data[save_adr+1]<<8| MMU_new.backupDevice.data[save_adr+0]<<0;

						MMU_new.backupDevice.setLazyFlushPending();

						save_adr += 4;
					}
					else
					{
						val = rom.read();
					}
				}
				break;

			// Nand Status?
			case 0xD6:
				//0x80 == busy
				//0x40 == ??
				//0x20 == ready?
				//0x10 == write enabled?
				switch (mode)
				{
					case 0x84: //Write disable
						val = 0x20202020;
						break;
					case 0x85: //Write enable
						val = 0x20202020 | 0x10101010;
						break;
					case 0x8B:
						val = 0x60606060 | 0x10101010;
						break;
					case 0xB2: //Set save position
						val = 0x20202020;
						break;
					default:
						val = 0x60606060; //0x20 == ready
						break;
				}
				break;
		}
		
		return val;
	}

	virtual void slot1client_write_GCDATAIN(eSlot1Operation operation, u32 val)
	{
		//pass the normal rom operations along to the rom component
		switch(operation)
		{
			case eSlot1Operation_00_ReadHeader_Unencrypted:
			case eSlot1Operation_B7_Read:
			case eSlot1Operation_2x_SecureAreaLoad:
				return;
		}

		//handle special commands ourselves
		int cmd = protocol.command.bytes[0];
		u32 value = val;
		u32 adr = save_adr;
		switch(cmd)
		{
			case 0x81:	//Nand Write
				MMU_new.backupDevice.ensure(adr+4,0);
				MMU_new.backupDevice.data[adr+0] = (val >>  0) & 0xFF;
				MMU_new.backupDevice.data[adr+1] = (val >>  8) & 0xFF;
				MMU_new.backupDevice.data[adr+2] = (val >> 16) & 0xFF;
				MMU_new.backupDevice.data[adr+3] = (val >> 24) & 0xFF;
				MMU_new.backupDevice.setFlushPending();
				save_adr += 4;
				break;
		}
	}

	virtual void savestate(EMUFILE* os)
	{
		protocol.savestate(os);
		rom.savestate(os);
	}

	virtual void loadstate(EMUFILE* is)
	{
		protocol.loadstate(is);
		rom.loadstate(is);
	}


};

ISlot1Interface* construct_Slot1_Retail_NAND() { return new Slot1_Retail_NAND(); }


