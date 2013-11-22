/*
	Copyright (C) 2013 DeSmuME team

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

#include "../slot2.h"
#include "../registers.h"
#include "../MMU.h"
#include "../NDSSystem.h"
#ifdef HOST_WINDOWS
#include "../windows/inputdx.h"
#endif

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

		NDS_SLOT2_TYPE selection = NDS_SLOT2_NONE;

		//check game ID in core emulator and select right implementation
		if (gameInfo.romsize == 0)
		{}
		else
		if ((memcmp(gameInfo.header.gameCode, "UBR",  3) == 0)) selection = NDS_SLOT2_EXPMEMORY; // Opera Browser
		else
			if ((memcmp(gameInfo.header.gameCode, "YGH",  3) == 0)) selection = NDS_SLOT2_GUITARGRIP; // Guitar Hero - On Tour
		else
			if ((memcmp(gameInfo.header.gameCode, "CGS",  3) == 0)) selection = NDS_SLOT2_GUITARGRIP; // Guitar Hero - On Tour - Decades
		else
			if ((memcmp(gameInfo.header.gameCode, "C6Q",  3) == 0)) selection = NDS_SLOT2_GUITARGRIP; // Guitar Hero - On Tour - Modern Hits
		else
			if ((memcmp(gameInfo.header.gameCode, "YGR",  3) == 0)) selection = NDS_SLOT2_GUITARGRIP; // Guitar Hero - On Tour (Demo)
		else
			if ((memcmp(gameInfo.header.gameCode, "Y56",  3) == 0)) selection = NDS_SLOT2_GUITARGRIP; // Guitar Hero - On Tour - Decades (Demo)
		else
			if ((memcmp(gameInfo.header.gameCode, "Y6R",  3) == 0)) selection = NDS_SLOT2_GUITARGRIP; // Guitar Hero - On Tour - Modern Hits (Demo)
		else
			if ((memcmp(gameInfo.header.gameCode, "BEP",  3) == 0)) selection = NDS_SLOT2_EASYPIANO; // Easy Piano (EUR)(USA)
		else
			if ((memcmp(gameInfo.header.gameCode, "YAA",  3) == 0)) selection = NDS_SLOT2_PADDLE; // Arkanoid DS
		else
			if ((memcmp(gameInfo.header.gameCode, "CB6",  3) == 0)) selection = NDS_SLOT2_PADDLE; // Space Bust-A-Move
		else
			if ((memcmp(gameInfo.header.gameCode, "YXX",  3) == 0)) selection = NDS_SLOT2_PADDLE; // Space Invaders Extreme
				else
			if ((memcmp(gameInfo.header.gameCode, "CV8",  3) == 0)) selection = NDS_SLOT2_PADDLE; // Space Invaders Extreme 2
		else
			if (gameInfo.isHomebrew())
				selection = NDS_SLOT2_PASSME;
		
		slot2_selected_type = selection;
		mSelectedImplementation = slot2_List[selection];
		mSelectedImplementation->connect();
		printf("Slot2 auto-selected device type: %s (0x%02X)\n", mSelectedImplementation->info()->name(), mSelectedImplementation->info()->id());
		
#ifdef HOST_WINDOWS
		Guitar.Enabled	= (selection == NDS_SLOT2_GUITARGRIP)?true:false;
		Piano.Enabled	= (selection == NDS_SLOT2_EASYPIANO)?true:false;
		Paddle.Enabled	= (selection == NDS_SLOT2_PADDLE)?true:false;
#endif
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
