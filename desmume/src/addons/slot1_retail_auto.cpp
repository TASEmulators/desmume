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

#include "../slot1.h"
#include "../registers.h"
#include "../MMU.h"
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
		static Slot1InfoSimple info("Retail (Auto)","Slot1 Retail (auto-selection) card emulation");
		return &info;
	}

	virtual void connect()
	{

		NDS_SLOT1_TYPE selection = NDS_SLOT1_RETAIL_MCROM;
		
		//check game ID in core emulator and select right implementation
		if(!memcmp(gameInfo.header.gameCode,"UORE",4) ||
			!memcmp(gameInfo.header.gameCode,"UORJ",4))
			selection = NDS_SLOT1_RETAIL_NAND;

		mSelectedImplementation = slot1_List[selection];
		mSelectedImplementation->connect();
		printf("slot1 auto-selected device type: %s\n",mSelectedImplementation->info()->name());

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
};

ISlot1Interface* construct_Slot1_Retail_Auto() { return new Slot1_Retail_Auto(); }

	//		///writetoGCControl:
	//// --- Ninja SD commands -------------------------------------

	//	// NJSD init/reset
	//	case 0x20:
	//		{
	//			card.address = 0;
	//			card.transfer_count = 0;
	//		}
	//		break;

	//	// NJSD_sendCLK()
	//	case 0xE0:
	//		{
	//			card.address = 0;
	//			card.transfer_count = 0;
	//			NDS_makeInt(PROCNUM, 20);
	//		}
	//		break;

	//	// NJSD_sendCMDN() / NJSD_sendCMDR()
	//	case 0xF0:
	//	case 0xF1:
	//		switch (card.command[2])
	//		{
	//		// GO_IDLE_STATE
	//		case 0x40:
	//			card.address = 0;
	//			card.transfer_count = 0;
	//			NDS_makeInt(PROCNUM, 20);
	//			break;

	//		case 0x42:  // ALL_SEND_CID
	//		case 0x43:  // SEND_RELATIVE_ADDR
	//		case 0x47:  // SELECT_CARD
	//		case 0x49:  // SEND_CSD
	//		case 0x4D:
	//		case 0x77:  // APP_CMD
	//		case 0x69:  // SD_APP_OP_COND
	//			card.address = 0;
	//			card.transfer_count = 6;
	//			NDS_makeInt(PROCNUM, 20);
	//			break;

	//		// SET_BLOCKLEN
	//		case 0x50:
	//			card.address = 0;
	//			card.transfer_count = 6;
	//			card.blocklen = card.command[6] | (card.command[5] << 8) | (card.command[4] << 16) | (card.command[3] << 24);
	//			NDS_makeInt(PROCNUM, 20);
	//			break;

	//		// READ_SINGLE_BLOCK
	//		case 0x51:
	//			card.address = card.command[6] | (card.command[5] << 8) | (card.command[4] << 16) | (card.command[3] << 24);
	//			card.transfer_count = (card.blocklen + 3) >> 2;
	//			NDS_makeInt(PROCNUM, 20);
	//			break;
	//		}
	//		break;

	//	// --- Ninja SD commands end ---------------------------------



	//		//GCDATAIN:
	//	// --- Ninja SD commands -------------------------------------

	//	// NJSD_sendCMDN() / NJSD_sendCMDR()
	//	case 0xF0:
	//	case 0xF1:
	//		switch (card.command[2])
	//		{
	//		// ALL_SEND_CID
	//		case 0x42:
	//			if (card.transfer_count == 2) val = 0x44534A4E;
	//			else val = 0x00000000;

	//		// SEND_RELATIVE_ADDR
	//		case 0x43:
	//		case 0x47:
	//		case 0x49:
	//		case 0x50:
	//			val = 0x00000000;
	//			break;

	//		case 0x4D:
	//			if (card.transfer_count == 2) val = 0x09000000;
	//			else val = 0x00000000;
	//			break;

	//		// APP_CMD
	//		case 0x77:
	//			if (card.transfer_count == 2) val = 0x00000037;
	//			else val = 0x00000000;
	//			break;

	//		// SD_APP_OP_COND
	//		case 0x69:
	//			if (card.transfer_count == 2) val = 0x00008000;
	//			else val = 0x00000000;
	//			break;

	//		// READ_SINGLE_BLOCK
	//		case 0x51:
	//			val = 0x00000000;
	//			break;
	//		}
	//		break;

	//	// --- Ninja SD commands end ---------------------------------


