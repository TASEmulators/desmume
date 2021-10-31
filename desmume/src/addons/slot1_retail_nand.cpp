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

// Games with NAND Flash:
// Ore/WarioWare D.I.Y. - chip:		SAMSUNG 004
//									KLC2811ANB-P204
//									NTR-UORE-0
//									- 128Mbit

#include "slot1comp_rom.h"
#include "slot1comp_protocol.h"

#include "../slot1.h"
#include "../NDSSystem.h"
#include "../emufile.h"

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

	//current NAND read/write start position
	//when this is changed, the read/write cursor will be reset to it
	//when it is set to the same value, the read/write cursor will NOT be reset
	//(this is since some value must necessarily come in on the protocol address, so the 'current save_start' is used as a special 'dont change' value
	u32 save_start;

	//current NAND read/write cursor
	u32 save_adr;

public:
	virtual Slot1Info const* info()
	{
		static Slot1InfoSimple info("Retail NAND","Slot1 retail NAND card emulation", 0x02);
		return &info;
	}

	virtual void connect()
	{
		protocol.reset(this);
		protocol.chipId = gameInfo.chipID;
		protocol.gameCode = T1ReadLong((u8*)gameInfo.header.gameCode,0);

		save_adr = 0;
		handle_save = 0;
		mode = 0;
		subAdr = T1ReadWord(gameInfo.header.reserved2, 0x6) << 17;
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

	virtual void slot1client_startOperation(eSlot1Operation theOperation)
	{
		//INFO("Start command: %02X%02X%02X%02X%02X%02X%02X%02X\t",
		//	protocol.command.bytes[0], protocol.command.bytes[1], protocol.command.bytes[2], protocol.command.bytes[3],
		//	protocol.command.bytes[4], protocol.command.bytes[5], protocol.command.bytes[6], protocol.command.bytes[7]);
		//INFO("FROM: %08X\n", NDS_ARM9.instruct_adr);

		u32 addressFromProtocol = (protocol.command.bytes[1] << 24) | (protocol.command.bytes[2] << 16) | (protocol.command.bytes[3] << 8) | protocol.command.bytes[4];

		//pass the normal rom operations along to the rom component
		switch (theOperation)
		{
			case eSlot1Operation_00_ReadHeader_Unencrypted:
				rom.start(theOperation, addressFromProtocol);
				return;

			case eSlot1Operation_2x_SecureAreaLoad:
				rom.start(theOperation, protocol.address);
				return;
				
			default:
				break;
		}

		//handle special commands ourselves
		int cmd = protocol.command.bytes[0];
		switch(cmd)
		{
			// Nand Init
			case 0x94:
				mode = cmd;
				break;

			// Nand Error?
			case 0xD6:
				break;

			//Nand Write Page
			case 0x81:
				mode = cmd;
				if(addressFromProtocol != save_start)
				{
					save_start = addressFromProtocol;
					save_adr = (addressFromProtocol & gameInfo.mask) - subAdr;
				}
				handle_save = 1;
				break;

			case 0x84: //Write disable
			case 0x85: //Write enable
				mode = cmd;
				break;
			case 0x8B:
				mode = cmd;
				handle_save = 0;
				MMU_new.backupDevice.flushBackup();
				break;

			//Start read mode
			case 0xB7:
				if (handle_save)
				{
					mode = cmd;
					if(addressFromProtocol != save_start)
					{
						save_start = addressFromProtocol;
						save_adr = (addressFromProtocol & gameInfo.mask) - subAdr;
					}
				}
				else
				{
					rom.start(theOperation, addressFromProtocol);
				}
				break;

			case 0xB2: //Set save position
				mode = cmd;
				save_start = addressFromProtocol;
				//cursor resets regardless of whether save_start changed, that's what makes this special.
				//the cursor could be reset to the beginning of the previous save_start region
				save_adr = (addressFromProtocol & gameInfo.mask) - subAdr;
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
				
			default:
				break;
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
						MMU_new.backupDevice.ensure(save_adr+4, (u8)0);

						val = MMU_new.backupDevice.readLong(save_adr, 0);

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
				
			default:
				break;
		}

		//handle special commands ourselves
		int cmd = protocol.command.bytes[0];
		u32 value = val;
		u32 adr = save_adr;
		switch(cmd)
		{
			case 0x81:	//Nand Write

				MMU_new.backupDevice.ensure(adr+4, (u8)0);
				MMU_new.backupDevice.writeLong(adr, val);

				save_adr += 4;
				break;
		}
	}

	virtual void post_fakeboot(int PROCNUM)
	{
		// The BIOS leaves the card in NORMAL mode
		protocol.mode = eCardMode_NORMAL;
	}

	virtual void savestate(EMUFILE &os)
	{
		s32 version = 0;

		protocol.savestate(os);
		rom.savestate(os);

		os.write_32LE(version);
		
		os.write_32LE(mode);
		os.write_32LE(handle_save);
		os.write_32LE(save_adr);
		os.write_32LE(save_start);
		os.write_32LE(subAdr);
	}

	virtual void loadstate(EMUFILE &is)
	{
		s32 version = 0;

		protocol.loadstate(is);
		rom.loadstate(is);

		is.read_32LE(version);

		// version 0
		if (version >= 0)
		{
			is.read_32LE(mode);
			is.read_32LE(handle_save);
			is.read_32LE(save_adr);
			is.read_32LE(save_start);
			is.read_32LE(subAdr);
		}
	}


};

ISlot1Interface* construct_Slot1_Retail_NAND() { return new Slot1_Retail_NAND(); }


