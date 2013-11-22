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

/*
Notes for future development:
Despite gbatek's specifications contrariwise, GCDATAIN is writeable. R4 uses this for writes to the card.
Despite gbatek's specifications contrariwise, GCROMCTRL[30] can take a value of 1 to indicate that the GC bus transaction should be writes.
It is unclear what kicks GC bus transactions. It's possibly bit 31 of GCROMCTRL, but normmatt thinks he might have had to hack around that to make something work, and that it might just be any write to GCROMCTRL.
Since GCROMCTRL[26:24] can't represent 'data block size' of 1 or 2, it is assumed that transactions always happen in 4 byte pieces.
...so, any 8/16bit accesses to GCDATAIN would transfer a whole 32bit unit and then just return the requested portion
*/

//TODO - create a Slot1_TurboRom which we can select when booting from fakebios/nonfirmware and which would provide a useful service to people porting to tighter platforms by bypassing largely useless cruft

#include <string>

#include "types.h"
#include "slot1.h"

#include "NDSSystem.h"
#include "emufile.h"
#include "utils/vfat.h"
#include "path.h"

bool slot1_R4_path_type = false;

//-------
//fat-related common elements
static EMUFILE* fatImage = NULL;
static std::string fatDir;

static void scanDir()
{
	if(fatDir == "") return;
	
	if (fatImage)
	{
		delete fatImage;
		fatImage = NULL;
	}

	VFAT vfat;
	if(vfat.build(slot1_R4_path_type?path.RomDirectory.c_str():fatDir.c_str(), 16))
	{
		fatImage = vfat.detach();
	}
}


void slot1_SetFatDir(const std::string& dir, bool sameAsRom)
{
	//printf("FAT path %s\n", dir.c_str());
	slot1_R4_path_type = sameAsRom;
	if (!slot1_R4_path_type)
		fatDir = dir;
}

std::string slot1_GetFatDir()
{
	return fatDir;
}

EMUFILE* slot1_GetFatImage()
{
	return fatImage;
}

//------------

ISlot1Interface* slot1_List[NDS_SLOT1_COUNT] = {0};

ISlot1Interface* slot1_device = NULL;
NDS_SLOT1_TYPE slot1_device_type = NDS_SLOT1_RETAIL_AUTO;  //default for frontends that dont even configure this
NDS_SLOT1_TYPE slot1_selected_type = NDS_SLOT1_NONE;


void slot1_Init()
{
	//due to sloppy initialization code in various untestable desmume ports, we might try this more than once
	static bool initialized = false;
	if(initialized) return;
	initialized = true;

	//construct all devices
	extern TISlot1InterfaceConstructor construct_Slot1_None;
	extern TISlot1InterfaceConstructor construct_Slot1_Retail_Auto;
	extern TISlot1InterfaceConstructor construct_Slot1_R4;
	extern TISlot1InterfaceConstructor construct_Slot1_Retail_NAND;
	extern TISlot1InterfaceConstructor construct_Slot1_Retail_MCROM;
	extern TISlot1InterfaceConstructor construct_Slot1_Retail_DEBUG;
	slot1_List[NDS_SLOT1_NONE] = construct_Slot1_None();
	slot1_List[NDS_SLOT1_RETAIL_AUTO] = construct_Slot1_Retail_Auto();
	slot1_List[NDS_SLOT1_R4] = construct_Slot1_R4();
	slot1_List[NDS_SLOT1_RETAIL_NAND] = construct_Slot1_Retail_NAND();
	slot1_List[NDS_SLOT1_RETAIL_MCROM] = construct_Slot1_Retail_MCROM();
	slot1_List[NDS_SLOT1_RETAIL_DEBUG] = construct_Slot1_Retail_DEBUG();
}

void slot1_Shutdown()
{
	for(int i=0;i<ARRAY_SIZE(slot1_List);i++)
	{
		if(slot1_List[i])
			slot1_List[i]->shutdown();
		delete slot1_List[i];
	}
}

bool slot1_Connect()
{
	slot1_device->connect();
	return true;
}

void slot1_Disconnect()
{
	slot1_device->disconnect();
	
	//be careful to do this second, maybe the device will write something more
	if (fatImage)
	{
		delete fatImage;
		fatImage = NULL;
	}
}

void slot1_Reset()
{
	//disconnect existing device
	if(slot1_device != NULL) slot1_device->disconnect();
	
	//connect new device
	slot1_device = slot1_List[slot1_device_type];
	if (slot1_device_type == NDS_SLOT1_R4)
		scanDir();
	slot1_device->connect();
}

bool slot1_Change(NDS_SLOT1_TYPE changeToType)
{
	if((changeToType == slot1_device_type) || (changeToType == slot1_GetSelectedType()))
		return FALSE; //nothing to do
	if (changeToType > NDS_SLOT1_COUNT || changeToType < 0) return FALSE;
	if(slot1_device != NULL)
		slot1_device->disconnect();
	slot1_device_type = changeToType;
	slot1_device = slot1_List[slot1_device_type];
	printf("Slot 1: %s\n", slot1_device->info()->name());
	printf("sending eject signal to SLOT-1\n");
	NDS_TriggerCardEjectIRQ();
	slot1_device->connect();
	return true;
}

bool slot1_getTypeByID(u8 ID, NDS_SLOT1_TYPE &type)
{
	for (u8 i = 0; i < NDS_SLOT1_COUNT; i++)
	{
		if (slot1_List[i]->info()->id() == ID)
		{
			type = (NDS_SLOT1_TYPE)i;
			return true;
		}
	}
	return false;
}

bool slot1_ChangeByID(u8 ID)
{
	NDS_SLOT1_TYPE type = NDS_SLOT1_RETAIL_AUTO;
	slot1_getTypeByID(ID, type);
	return slot1_Change(type);
}

NDS_SLOT1_TYPE slot1_GetCurrentType()
{
	return slot1_device_type;
}

NDS_SLOT1_TYPE slot1_GetSelectedType()
{
	if (slot1_device_type == NDS_SLOT1_RETAIL_AUTO)
		return slot1_selected_type;
	return slot1_device_type;
}

void slot1_Savestate(EMUFILE* os)
{
	slot1_device->savestate(os);
}
void slot1_Loadstate(EMUFILE* is)
{
	slot1_device->loadstate(is);
}

	//// --- Ninja SD commands notes -------------------------------------
	//		///writetoGCControl:

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


