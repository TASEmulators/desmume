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

//this file contains the components used for emulating standard gamecard protocol.
//this largely means the complex boot-up process.
//i think there's no reason why proprietary cards couldn't speak any protocol they wish, as long as they didn't mind being unbootable. 
//TODO - could this be refactored into a base class? that's probably more reasonable. but we've gone with this modular mix-in architecture so... not yet.

#ifndef _SLOT1COMP_PROTOCOL_H
#define _SLOT1COMP_PROTOCOL_H

#include "../types.h"
#include "../MMU.h"

class EMUFILE;

enum eSlot1Operation
{
	//----------
	//RAW mode operations
	//before encrypted communications can be established, some values from the rom header must be read.
	//this is the only way to read the header, actually, since the only reading commands available to games (after KEY2 mode is set) are 
	eSlot1Operation_00_ReadHeader_Unencrypted,
	//it's not clear why this exists
	eSlot1Operation_9F_Dummy,
	eSlot1Operation_90_ChipID,
	//----------

	//----------
	//KEY1 mode operations
	eSlot1Operation_1x_ChipID,
	eSlot1Operation_2x_SecureAreaLoad,
	//----------
	
	//----------
	//NORMAL mode operations
	//the main rom data reading command
	eSlot1Operation_B7_Read,
	eSlot1Operation_B8_ChipID,
	//----------

	eSlot1Operation_Unknown
};

class ISlot1Comp_Protocol_Client
{
public:
	virtual void slot1client_startOperation(eSlot1Operation operation) {}
	virtual u32 slot1client_read_GCDATAIN(eSlot1Operation operation) = 0;
	virtual void slot1client_write_GCDATAIN(eSlot1Operation operation, u32 val) {}
};


class Slot1Comp_Protocol
{
public:

	void savestate(EMUFILE* os);
	void loadstate(EMUFILE* is);
	
	//set some kind of protocol/hardware reset state
	void reset(ISlot1Comp_Protocol_Client* client);

	//signals from the GC bus
	void write_command(GC_Command command);
	void write_GCDATAIN(u8 PROCNUM, u32 val);
	u32 read_GCDATAIN(u8 PROCNUM);

	//helpers for write_command()
	void write_command_RAW(GC_Command command);
	void write_command_KEY1(GC_Command command);
	void write_command_NORMAL(GC_Command command);
	
	//operations not related to obscurities of the protocol or otherwise unknown are passed through to the client here
	ISlot1Comp_Protocol_Client* client;

	//--state--

	//the major operational mode. the protocol shifts modes and interprets commands into operations differently depending on the mode
	eCardMode mode;

	//the current operational state
	eSlot1Operation operation;

	//the command we're currently crunching on
	GC_Command command;

	//most operations are defined in terms of returning a series of bytes
	//the meaning of these varies by operation. they are provided publicly as a service to clients
	u32 address;
	s32 length, delay; //the expected length and delay of this state

	//chipId which should be returned by the various chipId commands
	u32 chipId;

	//gameCode used by the protocol KEY1 crypto
	u32 gameCode;
};

#endif //_SLOT1COMP_PROTOCOL_H