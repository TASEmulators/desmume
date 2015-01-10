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

#include "slot1comp_mc.h"
#include "slot1comp_rom.h"
#include "slot1comp_protocol.h"

#include "../slot1.h"
#include "../NDSSystem.h"

//quick architecture overview:
//MCROM receives GC bus commands from MMU.cpp
//those are passed on to the protocol component for parsing
//protocol calls back into MCROM via ISlot1Comp_Protocol_Client interface for things the protocol doesnt know about (the contents of the rom, chiefly)
//MCROM utilizes the rom component for address logic and delivering data

class Slot1_Retail_MCROM : public ISlot1Interface, public ISlot1Comp_Protocol_Client
{
private:
	Slot1Comp_Protocol protocol;
	Slot1Comp_Rom rom;

public:

	virtual Slot1Info const* info()
	{
		static Slot1InfoSimple info("Retail MC+ROM", "Slot1 Retail MC+ROM (standard) card emulation", 0x01);
		return &info;
	}

	virtual void connect()
	{
		protocol.reset(this);
		protocol.chipId = gameInfo.chipID;
		protocol.gameCode = T1ReadLong((u8*)gameInfo.header.gameCode,0);
		g_Slot1Comp_MC.connect();
	}

	virtual u8 auxspi_transaction(int PROCNUM, u8 value)
	{
		return g_Slot1Comp_MC.auxspi_transaction(PROCNUM,value);
	}

	virtual void auxspi_reset(int PROCNUM)
	{
		g_Slot1Comp_MC.auxspi_reset(PROCNUM);
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
		rom.start(operation,protocol.address);
	}

    virtual void post_fakeboot(int PROCNUM)
    {
        // The BIOS leaves the card in NORMAL mode
        protocol.mode = eCardMode_NORMAL;
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

private:

	u32 slot1client_read_GCDATAIN(eSlot1Operation operation)
	{
		return rom.read();
	}
};

ISlot1Interface* construct_Slot1_Retail_MCROM() { return new Slot1_Retail_MCROM(); }
