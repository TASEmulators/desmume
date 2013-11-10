/*
	Copyright (C) 2009-2013 DeSmuME team

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
#include "slot2.h"
#include "../types.h"
#include "../mem.h"
#include "../MMU.h"

//this is the currently-configured cflash mode
ADDON_CFLASH_MODE CFlash_Mode = ADDON_CFLASH_MODE_RomPath;

//this is the currently-configured path (directory or filename) for cflash.
//it should be viewed as a parameter for the above.
std::string CFlash_Path;

char GBAgameName[MAX_PATH] = {0};


ISlot2Interface* slot2_List[NDS_SLOT2_COUNT] = {0};

ISlot2Interface* slot2_device = NULL;
NDS_SLOT2_TYPE slot2_device_type = NDS_SLOT2_NONE;


void slot2_Init()
{
	//due to sloppy initialization code in various untestable desmume ports, we might try this more than once
	static bool initialized = false;
	if(initialized) return;
	initialized = true;

	//construct all devices
	extern TISlot2InterfaceConstructor construct_Slot2_None;
	//extern TISlot2InterfaceConstructor construct_Slot2_Auto;
	extern TISlot2InterfaceConstructor construct_Slot2_CFlash;
	extern TISlot2InterfaceConstructor construct_Slot2_RumblePak;
	extern TISlot2InterfaceConstructor construct_Slot2_GbaCart;
	extern TISlot2InterfaceConstructor construct_Slot2_GuitarGrip;
	extern TISlot2InterfaceConstructor construct_Slot2_ExpansionPak;
	extern TISlot2InterfaceConstructor construct_Slot2_EasyPiano;
	extern TISlot2InterfaceConstructor construct_Slot2_Paddle;
	extern TISlot2InterfaceConstructor construct_Slot2_PassME;

	slot2_List[NDS_SLOT2_NONE]			= construct_Slot2_None();
	//slot2_List[NDS_SLOT2_AUTO]		= construct_Slot2_Auto();
	slot2_List[NDS_SLOT2_CFLASH]		= construct_Slot2_CFlash();
	slot2_List[NDS_SLOT2_RUMBLEPAK]		= construct_Slot2_RumblePak();
	slot2_List[NDS_SLOT2_GBACART]		= construct_Slot2_GbaCart();
	slot2_List[NDS_SLOT2_GUITARGRIP]	= construct_Slot2_GuitarGrip();
	slot2_List[NDS_SLOT2_EXPMEMORY]		= construct_Slot2_ExpansionPak();
	slot2_List[NDS_SLOT2_EASYPIANO]		= construct_Slot2_EasyPiano();
	slot2_List[NDS_SLOT2_PADDLE]		= construct_Slot2_Paddle();
	slot2_List[NDS_SLOT2_PASSME]		= construct_Slot2_PassME();

}

void slot2_Shutdown()
{
	for(int i=0; i<ARRAY_SIZE(slot2_List); i++)
	{
		if(slot2_List[i])
			slot2_List[i]->shutdown();
		delete slot2_List[i];
		slot2_List[i] = NULL;
	}
}

bool slot2_Connect()
{
	slot2_device->connect();
	return true;
}

void slot2_Disconnect()
{
	slot2_device->disconnect();
}

void slot2_Reset()
{
	//disconnect existing device
	if(slot2_device != NULL) slot2_device->disconnect();
	
	//connect new device
	slot2_device = slot2_List[slot2_device_type];
	slot2_device->connect();
}

bool slot2_Change(NDS_SLOT2_TYPE changeToType)
{
	if(changeToType == slot2_device_type)
		return FALSE; //nothing to do
	if (changeToType > NDS_SLOT2_COUNT || changeToType < 0)
		return FALSE;
	if(slot2_device != NULL)
		slot2_device->disconnect();

	slot2_device_type = changeToType;
	slot2_device = slot2_List[slot2_device_type];
	printf("Slot 2: %s\n", slot2_device->info()->name());
	slot2_device->connect();
	return true;
}

NDS_SLOT2_TYPE slot2_GetCurrentType()
{
	return slot2_device_type;
}

void slot2_Savestate(EMUFILE* os)
{
	slot2_device->savestate(os);
}

void slot2_Loadstate(EMUFILE* is)
{
	slot2_device->loadstate(is);
}

static bool isSlot2(u32 addr)
{
	if (addr < 0x08000000) return false;
	if (addr >= 0x0A010000) return false;
	
	return true;
}

template <u8 PROCNUM>
static bool skipSlot2Data()
{
	u16 exmemcnt = T1ReadWord(MMU.MMU_MEM[PROCNUM][0x40], 0x204);
	return (PROCNUM == ARMCPU_ARM9)? (exmemcnt & EXMEMCNT_MASK_SLOT2_ARM7):
									!(exmemcnt & EXMEMCNT_MASK_SLOT2_ARM7);
}

template <u8 PROCNUM, typename T>
bool slot2_write(u32 addr, T val)
{
	if (!isSlot2(addr)) return false;
	if (skipSlot2Data<PROCNUM>()) return true;

	switch (sizeof(T))
	{
		case sizeof(u8) : slot2_device->writeByte(PROCNUM, addr, val); break;
		case sizeof(u16): slot2_device->writeWord(PROCNUM, addr, val); break;
		case sizeof(u32): slot2_device->writeLong(PROCNUM, addr, val); break;
	}
	return true;
}

template <u8 PROCNUM, typename T>
bool slot2_read(u32 addr, T &val)
{
	if (!isSlot2(addr)) return false;
	if (skipSlot2Data<PROCNUM>()) { val = 0; return true; }

	switch (sizeof(T))
	{
		case sizeof(u8) : val = slot2_device->readByte(PROCNUM, addr); break;
		case sizeof(u16): val = slot2_device->readWord(PROCNUM, addr); break;
		case sizeof(u32): val = slot2_device->readLong(PROCNUM, addr); break;
		default: val = 0; break;
	}

	return true;
}

template bool slot2_write<0, u8> (u32 addr, u8  val);
template bool slot2_write<0, u16>(u32 addr, u16 val);
template bool slot2_write<0, u32>(u32 addr, u32 val);
template bool slot2_write<1, u8> (u32 addr, u8  val);
template bool slot2_write<1, u16>(u32 addr, u16 val);
template bool slot2_write<1, u32>(u32 addr, u32 val);

template bool slot2_read<0, u8> (u32 addr, u8  &val);
template bool slot2_read<0, u16>(u32 addr, u16 &val);
template bool slot2_read<0, u32>(u32 addr, u32 &val);
template bool slot2_read<1, u8> (u32 addr, u8  &val);
template bool slot2_read<1, u16>(u32 addr, u16 &val);
template bool slot2_read<1, u32>(u32 addr, u32 &val);

