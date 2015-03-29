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

#ifndef __SLOT1_H__
#define __SLOT1_H__

#include <string>

#include "types.h"
#include "MMU.h"

class EMUFILE;

class Slot1Info
{
public:
	virtual const char* name() const = 0;
	virtual const char* descr()const  = 0;
	virtual const u8 id() const  = 0;
};

class Slot1InfoSimple : public Slot1Info
{
public:
	Slot1InfoSimple(const char* _name, const char* _descr, const u8 _id)
		: mName(_name)
		, mDescr(_descr)
		, mID(_id)
	{
	}
	virtual const char* name() const { return mName; }
	virtual const char* descr() const { return mDescr; }
	virtual const u8 id() const { return mID; }
private:
	const char* mName, *mDescr;
	const u8 mID;
};

class ISlot1Interface
{
public:
	//called to get info about device (description)
	virtual Slot1Info const* info() = 0;

	//called once when the emulator starts up, or when the device springs into existence
	virtual bool init() { return true; }
	
	//called when the emulator connects the device
	virtual void connect() { }

	//called when the emulator disconnects the device
	virtual void disconnect() { }
	
	//called when the emulator shuts down, or when the device disappears from existence
	virtual void shutdown() { }

	//called then the cpu begins a new command/block on the GC bus
	virtual void write_command(u8 PROCNUM, GC_Command command) { }

	//called when the cpu writes to the GC bus
	virtual void write_GCDATAIN(u8 PROCNUM, u32 val) { }

	//called when the cpu reads from the GC bus
	virtual u32 read_GCDATAIN(u8 PROCNUM) { return 0xFFFFFFFF; }

	//transfers a byte to the slot-1 device via auxspi, and returns the incoming byte
	//cpu is provided for diagnostic purposes only.. the slot-1 device wouldn't know which CPU it is.
	virtual u8 auxspi_transaction(int PROCNUM, u8 value) { return 0x00; }

	//called when the auxspi burst is ended (SPI chipselect in is going low)
	virtual void auxspi_reset(int PROCNUM) {}
    
	//called when NDS_FakeBoot terminates, emulate in here the BIOS behaviour
	virtual void post_fakeboot(int PROCNUM) {}

	virtual void savestate(EMUFILE* os) {}

	virtual void loadstate(EMUFILE* is) {}
}; 

typedef ISlot1Interface* TISlot1InterfaceConstructor();

enum NDS_SLOT1_TYPE
{
	NDS_SLOT1_NONE,			// 0xFF - None
	NDS_SLOT1_RETAIL_AUTO,	// 0xFE - autodetect which kind of retail card to use 
	NDS_SLOT1_R4,			// 0x03 - R4 flash card
	NDS_SLOT1_RETAIL_NAND,	// 0x02 - Made in Ore/WarioWare D.I.Y.
	NDS_SLOT1_RETAIL_MCROM,	// 0x01 - a standard MC (eeprom, flash, fram) -bearing retail card. Also supports motion, for now, because that's the way we originally coded it
	NDS_SLOT1_RETAIL_DEBUG,	// 0x04 - for romhacking and fan-made translations
	NDS_SLOT1_COUNT			//use to count addons - MUST BE LAST!!!
};

extern ISlot1Interface* slot1_device;						//the current slot1 device instance
extern ISlot1Interface* slot1_List[NDS_SLOT1_COUNT];
extern NDS_SLOT1_TYPE slot1_selected_type;

void slot1_Init();
bool slot1_Connect();
void slot1_Disconnect();
void slot1_Shutdown();
void slot1_Savestate(EMUFILE* os);
void slot1_Loadstate(EMUFILE* is);

//just disconnects and reconnects the device. ideally, the disconnection and connection would be called with sensible timing
void slot1_Reset();

bool slot1_getTypeByID(u8 ID, NDS_SLOT1_TYPE &type);

//change the current device
bool slot1_Change(NDS_SLOT1_TYPE type);

//change the current device by ID
bool slot1_ChangeByID(u8 ID);

//check on the current device
NDS_SLOT1_TYPE slot1_GetCurrentType();
NDS_SLOT1_TYPE slot1_GetSelectedType();

extern bool slot1_R4_path_type;
void slot1_SetFatDir(const std::string& dir, bool sameAsRom = false);
std::string slot1_GetFatDir();
EMUFILE* slot1_GetFatImage();


#endif //__SLOT1_H__
