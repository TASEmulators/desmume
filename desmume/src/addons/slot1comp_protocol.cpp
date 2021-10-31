/*
	Copyright (C) 2012-2021 DeSmuME team

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

#include "slot1comp_protocol.h"

#include <string.h>

#include "../armcpu.h"
#include "../encrypt.h"
#include "../emufile.h"
#include "../utils/decrypt/decrypt.h"

static _KEY1 key1((const u8*)arm7_key);

void Slot1Comp_Protocol::reset(ISlot1Comp_Protocol_Client* theClient)
{
	this->client = theClient;

	//we have to initialize this to something.. lets use dummy.
	//(need to clean this up...)
	memcpy(&command,"\x9F\0\0\0\0\0\0\0",8); 
	operation = eSlot1Operation_9F_Dummy;

	length = 0;
	delay = 0;
	mode = eCardMode_RAW;
}

void Slot1Comp_Protocol::write_command_RAW(GC_Command theCommand)
{
	int cmd = theCommand.bytes[0];
	if(cmd == 0x9F)
	{
		operation = eSlot1Operation_9F_Dummy;
		length = 0x2000;
	}
	if(cmd == 0x90)
	{
		operation = eSlot1Operation_90_ChipID;
		length = 4;
		//we handle this operation ourselves
	}
	if(cmd == 0x3C)
	{
		//switch to KEY1
		length = 0; 
		mode = eCardMode_KEY1;
		
		//defer initialization of KEY1 until we know we need it, just to save some CPU time.
		//TODO - some information about these parameters
		//level == 2
		//modulo == 8
		key1.init(gameCode, 2, 0x08);
		GCLOG("[GC] KEY1 ACTIVATED\n");
	}
	if(cmd == 0x00)
	{
		operation = eSlot1Operation_00_ReadHeader_Unencrypted;
		client->slot1client_startOperation(operation);
	}
}

void Slot1Comp_Protocol::write_command_KEY1(GC_Command theCommand)
{
	//decrypt the KEY1-format command
	u32 temp[2];
	theCommand.toCryptoBuffer(temp);
	key1.decrypt(temp);
	theCommand.fromCryptoBuffer(temp);
	GCLOG("[GC] (key1-decrypted):"); theCommand.print();

	//and process it:
	int cmd = theCommand.bytes[0];
	switch(cmd&0xF0)
	{
		case 0x10:
			operation = eSlot1Operation_1x_ChipID;
			delay = 0x910, length = 4;
			//we handle this operation ourselves
			break;

		case 0x20:
			operation = eSlot1Operation_2x_SecureAreaLoad;
			delay = 0x910, length = 0x11A8;
			
			//TODO - more endian-safe way of doing this (theres examples in R4)
			{
#ifdef MSB_FIRST
				u64 cmd64 = *(u64*)theCommand.bytes;
#else
				u64 cmd64 = bswap64(*(u64*)theCommand.bytes);
#endif
				//todo - parse into blocknumber
				u32 blocknumber = (cmd64>>44)&0xFFFF;
				if(blocknumber<4||blocknumber>7)
					printf("SLOT1 WARNING: INVALID BLOCKNUMBER FOR \"Get Secure Area Block\": 0x%04X\n",blocknumber);
				address = blocknumber*0x1000;
			}
			client->slot1client_startOperation(operation);
			break;

		case 0x40:
			//switch to KEY2
			delay = 0x910, length = 0; 
			//well.. not really... yet.
			GCLOG("[GC] KEY2 ACTIVATED\n");
			break;

		case 0x60:
			//KEY2 disable? any info?
			break;

		case 0xA0:
			delay = 0x910, length = 0;
			mode = eCardMode_NORMAL;
			GCLOG("[GC] NORMAL MODE ACTIVATED\n");
			break;

	}
}

void Slot1Comp_Protocol::write_command_NORMAL(GC_Command theCommand)
{
	switch(command.bytes[0])
	{
	case 0xB7:
		{
			operation = eSlot1Operation_B7_Read;

			//TODO - more endian-safe way of doing this (theres examples in R4)
#ifdef MSB_FIRST
			u64 cmd64 = *(u64*)theCommand.bytes;
#else
			u64 cmd64 = bswap64(*(u64*)theCommand.bytes);
#endif
			address = (u32)((cmd64 >> 24));
			length = 0x200;

			client->slot1client_startOperation(operation);
		}
		break;

	case 0xB8:
		operation = eSlot1Operation_B8_ChipID;
		delay = 0, length = 4;
		//we handle this operation ourselves
		break;

	default:
		operation = eSlot1Operation_Unknown;
		client->slot1client_startOperation(operation);
		break;
	}
}

void Slot1Comp_Protocol::write_command(GC_Command theCommand)
{
	this->command = theCommand;

	//unrecognized commands will do something depending on the current state of the card
	delay = 0;
	length = 0;
	address = 0;

	switch(mode)
	{
		case eCardMode_RAW:
			write_command_RAW(theCommand);
			break;

		case eCardMode_KEY1:
			write_command_KEY1(theCommand);
			break;
	
		case eCardMode_NORMAL:
			write_command_NORMAL(theCommand);
			break;
			
		default:
			break;
	}
}

void Slot1Comp_Protocol::write_GCDATAIN(u8 PROCNUM, u32 val)
{
	switch(operation)
	{
		case eSlot1Operation_Unknown:
			client->slot1client_write_GCDATAIN(operation,val);
			break;
			
		default:
			break;
	}
}

u32 Slot1Comp_Protocol::read_GCDATAIN(u8 PROCNUM)
{
	switch(operation)
	{
		default:
			return client->slot1client_read_GCDATAIN(operation);

		case eSlot1Operation_9F_Dummy:
			return 0xFFFFFFFF;

		case eSlot1Operation_1x_ChipID:
			return chipId;

		case eSlot1Operation_90_ChipID:
		case eSlot1Operation_B8_ChipID:
			
			//Most games continuously compare the current chipId with the value in 
			//stored in memory at boot-up, probably to know if the card was removed.

			//staff of kings verifies this (it also uses the arm7 IRQ 20 to detect card ejects)
			return chipId;
	}

	return 0xFFFFFFFF;
}

void Slot1Comp_Protocol::savestate(EMUFILE &os)
{
	s32 version = 0;
	os.write_32LE(version);
	os.write_32LE((s32)mode);
	os.write_32LE((s32)operation);
	os.fwrite(command.bytes,8);
	os.write_32LE(address);
	os.write_32LE(length);
	os.write_32LE(delay);
	os.write_32LE(chipId);
	os.write_32LE(gameCode);
}

void Slot1Comp_Protocol::loadstate(EMUFILE &is)
{
	s32 version = is.read_s32LE();
	mode = (eCardMode)is.read_s32LE();
	operation = (eSlot1Operation)is.read_s32LE();
	is.fread(command.bytes,8);
	is.read_32LE(address);
	is.read_32LE(length);
	is.read_32LE(delay);
	is.read_32LE(chipId);
	is.read_32LE(gameCode);
}
