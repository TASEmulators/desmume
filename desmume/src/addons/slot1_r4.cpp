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

#include "slot1comp_protocol.h"

#include <time.h>

#include "../slot1.h"
#include "../NDSSystem.h"
#include "../emufile.h"

class Slot1_R4 : public ISlot1Interface, public ISlot1Comp_Protocol_Client
{
private:
	EMUFILE *img;
	Slot1Comp_Protocol protocol;
	u32 write_count;
	u32 write_enabled;

public:
	Slot1_R4()
		: img(NULL)
		, write_count(0)
		, write_enabled(0)
	{
	}

	virtual Slot1Info const* info()
	{
		static Slot1InfoSimple info("R4", "Slot1 R4 emulation", 0x03);
		return &info;
	}

	//called once when the emulator starts up, or when the device springs into existence
	virtual bool init()
	{
		//strange to do this here but we need to make sure its done at some point
		srand(time(NULL));
		return true;
	}
	
	virtual void connect()
	{
		img = slot1_GetFatImage();

		if(!img)
			INFO("slot1 fat not successfully mounted\n");

		protocol.reset(this);
		protocol.chipId = 0xFC2;
		protocol.gameCode = T1ReadLong((u8*)gameInfo.header.gameCode,0);
	}

	//called when the emulator disconnects the device
	virtual void disconnect()
	{
		img = NULL;
	}
	
	//called when the emulator shuts down, or when the device disappears from existence
	virtual void shutdown()
	{ 
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
		if(operation != eSlot1Operation_Unknown)
			return;

		u32 address;
		int cmd = protocol.command.bytes[0];
		switch(cmd)
		{
			case 0xB0:
				break;
			case 0xB9:
			case 0xBA:
				address = (protocol.command.bytes[1] << 24) | (protocol.command.bytes[2] << 16) | (protocol.command.bytes[3] << 8) | protocol.command.bytes[4];
				img->fseek(address,SEEK_SET);
				break;
			case 0xBB:
				write_enabled = 1;
				write_count = 0x80;
				//passthrough on purpose?
			case 0xBC:
				address = 	(protocol.command.bytes[1] << 24) | (protocol.command.bytes[2] << 16) | (protocol.command.bytes[3] << 8) | protocol.command.bytes[4];
				img->fseek(address,SEEK_SET);
				break;
		}
	}

	virtual u32 slot1client_read_GCDATAIN(eSlot1Operation operation)
	{
		if(operation != eSlot1Operation_Unknown)
			return 0;

		u32 val;
		int cmd = protocol.command.bytes[0];
		switch(cmd)
		{
			case 0xB0:
				val = (img) ? 0x1F4 : 0x1F2;
				break;
			case 0xB9:
				val = (rand() % 100) ? (img) ? 0x1F4 : 0x1F2 : 0;
				break;
			case 0xBB:
			case 0xBC:
				val = 0;
				break;
			case 0xBA:
				//INFO("Read from sd at sector %08X at adr %08X ",card.address/512,ftell(img));
				img->fread(&val, 4);
				//INFO("val %08X\n",val);
				break;
			default:
				val = 0;
				break;
		}

		return val;
	}

	void slot1client_write_GCDATAIN(eSlot1Operation operation, u32 val)
	{
		if(operation != eSlot1Operation_Unknown)
			return;

		int cmd = protocol.command.bytes[0];
		switch(cmd)
		{
			case 0xBB:
			{
				if(write_count && write_enabled)
				{
					img->fwrite(&val, 4);
					img->fflush();
					write_count--;
				}
				break;
			}
			default:
				break;
		}
	}

    virtual void post_fakeboot(int PROCNUM)
    {
        // The BIOS leaves the card in NORMAL mode
        protocol.mode = eCardMode_NORMAL;
    }

	void write32_GCDATAIN(u32 val)
	{
		//bool log = false;

		//last_write_count = write_count;

		//can someone tell me ... what the hell is this doing, anyway?
		//seems odd to use card.command[4] for this... isnt it part of the address?
		if(protocol.command.bytes[4])
		{
			// transfer is done
			//are you SURE this is logical? there doesnt seem to be any way for the card to signal that
			T1WriteLong(MMU.MMU_MEM[0][0x40], 0x1A4,val & 0x7F7FFFFF);

			MMU_GC_endTransfer(0);

			return;
		}

		int cmd = protocol.command.bytes[0];
		switch(cmd)
		{
			case 0xBB:
			{
				if(write_count && write_enabled)
				{
					img->fwrite(&val, 4);
					img->fflush();
					write_count--;
				}
				break;
			}
			default:
				break;
		}

		if(write_count==0)
		{
			write_enabled = 0;

			//transfer is done

			//are you SURE this is logical? there doesnt seem to be any way for the card to signal that
			T1WriteLong(MMU.MMU_MEM[0][0x40], 0x1A4,val & 0x7F7FFFFF);

			//but isnt this a different IRQ? IREQ_MC perhaps
			MMU_GC_endTransfer(0);
		}

		/*if(log)
		{
			INFO("WRITE CARD command: %02X%02X%02X%02X%02X%02X%02X%02X\t", 
							card.command[0], card.command[1], card.command[2], card.command[3],
							card.command[4], card.command[5], card.command[6], card.command[7]);
			INFO("FROM: %08X\t", NDS_ARM9.instruct_adr);
			INFO("VAL: %08X\n", val);
		}*/
	}

};

ISlot1Interface* construct_Slot1_R4() { return new Slot1_R4(); }
