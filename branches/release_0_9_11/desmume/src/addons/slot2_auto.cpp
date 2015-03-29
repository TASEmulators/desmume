/*
	Copyright (C) 2013-2015 DeSmuME team

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

#include <stdio.h>

#include "../slot2.h"

class Slot2_Auto : public ISlot2Interface
{
private:
	ISlot2Interface *mSelectedImplementation;

public:
	Slot2_Auto()
		: mSelectedImplementation(NULL)
	{
	}

	virtual Slot2Info const* info()
	{
		static Slot2InfoSimple info("Auto","Slot2 (auto-selection) device emulation", 0xFE);
		return &info;
	}
	
	virtual void connect()
	{
		slot2_selected_type = slot2_DetermineType();
		mSelectedImplementation = slot2_List[slot2_selected_type];
		mSelectedImplementation->connect();
		printf("Slot2 auto-selected device type: %s (0x%02X)\n", mSelectedImplementation->info()->name(), mSelectedImplementation->info()->id());
	}

	virtual void disconnect()
	{
		if(mSelectedImplementation) mSelectedImplementation->disconnect();
		mSelectedImplementation = NULL;
	}

	virtual void writeByte(u8 PROCNUM, u32 addr, u8 val) { mSelectedImplementation->writeByte(PROCNUM, addr, val); }
	virtual void writeWord(u8 PROCNUM, u32 addr, u16 val) { mSelectedImplementation->writeWord(PROCNUM, addr, val); }
	virtual void writeLong(u8 PROCNUM, u32 addr, u32 val) { mSelectedImplementation->writeLong(PROCNUM, addr, val); }

	virtual u8	readByte(u8 PROCNUM, u32 addr) { return mSelectedImplementation->readByte(PROCNUM, addr); }
	virtual u16	readWord(u8 PROCNUM, u32 addr) { return mSelectedImplementation->readWord(PROCNUM, addr); }
	virtual u32	readLong(u8 PROCNUM, u32 addr) { return mSelectedImplementation->readLong(PROCNUM, addr); }

	virtual void savestate(EMUFILE* os)
	{
		mSelectedImplementation->savestate(os);
	}

	virtual void loadstate(EMUFILE* is)
	{
		mSelectedImplementation->loadstate(is);
	}
};

ISlot2Interface* construct_Slot2_Auto() { return new Slot2_Auto(); }
