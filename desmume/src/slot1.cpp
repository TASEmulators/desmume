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

#include <string>

#include "types.h"
#include "slot1.h"

#include "NDSSystem.h"
#include "emufile.h"
#include "utils/vfat.h"

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
	if(vfat.build(fatDir.c_str(),16))
	{
		fatImage = vfat.detach();
	}
}


void slot1_SetFatDir(const std::string& dir)
{
	//printf("FAT path %s\n", dir.c_str());
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

ISlot1Interface* slot1_List[NDS_SLOT1_COUNT];

ISlot1Interface* slot1_device = NULL;
NDS_SLOT1_TYPE slot1_device_type = NDS_SLOT1_RETAIL;  //default for frontends that dont even configure this


void slot1_Init()
{
	//due to sloppy initialization code in various untestable desmume ports, we might try this more than once
	static bool initialized = false;
	if(initialized) return;
	initialized = true;

	//construct all devices
	extern TISlot1InterfaceConstructor construct_Slot1_None;
	extern TISlot1InterfaceConstructor construct_Slot1_Retail;
	extern TISlot1InterfaceConstructor construct_Slot1_R4;
	extern TISlot1InterfaceConstructor construct_Slot1_Retail_NAND;
	slot1_List[NDS_SLOT1_NONE] = construct_Slot1_None();
	slot1_List[NDS_SLOT1_RETAIL] = construct_Slot1_Retail();
	slot1_List[NDS_SLOT1_R4] = construct_Slot1_R4();
	slot1_List[NDS_SLOT1_RETAIL_NAND] = construct_Slot1_Retail_NAND();
}

void slot1_Shutdown()
{
	for(int i=0;i<ARRAY_SIZE(slot1_List);i++)
	{
		slot1_List[i]->shutdown();
		delete slot1_List[i];
	}
}

bool slot1_Connect()
{
	if (slot1_device_type == NDS_SLOT1_R4)
		scanDir();
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
	slot1_device->connect();
}

bool slot1_Change(NDS_SLOT1_TYPE changeToType)
{
	if(changeToType == slot1_device_type) return FALSE; //nothing to do
	if (changeToType > NDS_SLOT1_COUNT || changeToType < 0) return FALSE;
	if(slot1_device != NULL)
		slot1_device->disconnect();
	slot1_device_type = changeToType;
	slot1_device = slot1_List[slot1_device_type];
	if (changeToType == NDS_SLOT1_R4)
		scanDir();
	printf("Slot 1: %s\n", slot1_device->info()->name());
	printf("sending eject signal to SLOT-1\n");
	NDS_TriggerCardEjectIRQ();
	slot1_device->connect();
	return true;
}

NDS_SLOT1_TYPE slot1_GetCurrentType()
{
	return slot1_device_type;
}