/*
	Copyright (C) 2013-2105 DeSmuME team

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
#include "../NDSSystem.h"

class Slot1_Retail_Auto : public ISlot1Interface
{
private:
	ISlot1Interface *mSelectedImplementation;

public:
	Slot1_Retail_Auto()
		: mSelectedImplementation(NULL)
	{
	}

	virtual Slot1Info const* info()
	{
		static Slot1InfoSimple info("Retail (Auto)","Slot1 Retail (auto-selection) card emulation", 0xFE);
		return &info;
	}

	virtual void connect()
	{

		NDS_SLOT1_TYPE selection = NDS_SLOT1_RETAIL_MCROM;
		
		//check game ID in core emulator and select right implementation
		if ((memcmp(gameInfo.header.gameCode, "UOR",  3) == 0) ||	// WarioWare - D.I.Y. (U)(E)(EUR) / Made in Ore (J)
			(memcmp(gameInfo.header.gameCode, "UXBP", 4) == 0) 		// Jam with the Band (EUR)
			)
			selection = NDS_SLOT1_RETAIL_NAND;

		slot1_selected_type = selection;
		mSelectedImplementation = slot1_List[selection];
		mSelectedImplementation->connect();
		printf("Slot1 auto-selected device type: %s\n",mSelectedImplementation->info()->name());
	}

	virtual void disconnect()
	{
		if(mSelectedImplementation) mSelectedImplementation->disconnect();
		mSelectedImplementation = NULL;
	}

	virtual void write_command(u8 PROCNUM, GC_Command command)
	{
		mSelectedImplementation->write_command(PROCNUM, command);
	}

	virtual void write_GCDATAIN(u8 PROCNUM, u32 val)
	{
		mSelectedImplementation->write_GCDATAIN(PROCNUM, val);
	}

	virtual u32 read_GCDATAIN(u8 PROCNUM)
	{
		return mSelectedImplementation->read_GCDATAIN(PROCNUM);
	}

	virtual u8 auxspi_transaction(int PROCNUM, u8 value)
	{
		return mSelectedImplementation->auxspi_transaction(PROCNUM, value);
	}

	virtual void auxspi_reset(int PROCNUM)
	{
		mSelectedImplementation->auxspi_reset(PROCNUM);
	}

	virtual void post_fakeboot(int PROCNUM)
	{
		mSelectedImplementation->post_fakeboot(PROCNUM);
	}

	virtual void savestate(EMUFILE* os)
	{
		mSelectedImplementation->savestate(os);
	}

	virtual void loadstate(EMUFILE* is)
	{
		mSelectedImplementation->loadstate(is);
	}
};

ISlot1Interface* construct_Slot1_Retail_Auto() { return new Slot1_Retail_Auto(); }
