/*
	Copyright (C) 2010-2015 DeSmuME team

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
	u32 save_adr;
	u32 save_start_adr;

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

		save_start_adr = 0;
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

	virtual void slot1client_startOperation(eSlot1Operation operation)
	{
		//INFO("Start command: %02X%02X%02X%02X%02X%02X%02X%02X\t",
		//	protocol.command.bytes[0], protocol.command.bytes[1], protocol.command.bytes[2], protocol.command.bytes[3],
		//	protocol.command.bytes[4], protocol.command.bytes[5], protocol.command.bytes[6], protocol.command.bytes[7]);
		//INFO("FROM: %08X\n", NDS_ARM9.instruct_adr);

		//pass the normal rom operations along to the rom component
		switch(operation)
		{
			case eSlot1Operation_00_ReadHeader_Unencrypted:
				protocol.address = (protocol.command.bytes[1] << 24) | (protocol.command.bytes[2] << 16) | (protocol.command.bytes[3] << 8) | protocol.command.bytes[4];
				rom.start(operation,protocol.address);
				break;

			//case eSlot1Operation_B7_Read:  //zero 15-sep-2014 - this was removed during epoch of addon re-engineering to fix a bug

			case eSlot1Operation_2x_SecureAreaLoad:
				//don't re-generate address here. it was already done, according to different rules, for this operation
				rom.start(operation,protocol.address);
				return;
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
				if (save_start_adr != (protocol.address & gameInfo.mask) - subAdr)
				{
					save_start_adr = save_adr = (protocol.address & gameInfo.mask) - subAdr;
					MMU_new.backupDevice.seek(save_start_adr);
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

			case 0xB7:
				if (handle_save)
				{
					mode = cmd;
					if (save_start_adr != (protocol.address & gameInfo.mask) - subAdr)
					{
						save_start_adr = save_adr = (protocol.address & gameInfo.mask) - subAdr;
						MMU_new.backupDevice.seek(save_start_adr);
					}
				}
				else
				{
					rom.start(operation, protocol.address);
				}
				break;

			case 0xB2: //Set save position
				mode = cmd;
				save_start_adr = save_adr = (protocol.address & gameInfo.mask) - subAdr;
				MMU_new.backupDevice.seek(save_start_adr);
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

	virtual void savestate(EMUFILE* os)
	{
		s32 version = 0;

		protocol.savestate(os);
		rom.savestate(os);

		os->write32le(version);
		
		os->write32le(mode);
		os->write32le(handle_save);
		os->write32le(save_adr);
		os->write32le(save_start_adr);
		os->write32le(subAdr);
	}

	virtual void loadstate(EMUFILE* is)
	{
		s32 version = 0;

		protocol.loadstate(is);
		rom.loadstate(is);

		is->read32le(&version);

		// version 0
		if (version >= 0)
		{
			is->read32le(&mode);
			is->read32le(&handle_save);
			is->read32le(&save_adr);
			is->read32le(&save_start_adr);
			is->read32le(&subAdr);
		}
	}


};

ISlot1Interface* construct_Slot1_Retail_NAND() { return new Slot1_Retail_NAND(); }


