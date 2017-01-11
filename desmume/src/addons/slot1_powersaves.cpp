/*
	Copyright (C) 2017 DeSmuME team

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

//do not compile this file unless IFDEF HAVE_POWERSAVES

//tested on model AS233

#include <hidapi/hidapi.h>

#include "../slot1.h"
#include "../NDSSystem.h"
#include "slot1comp_protocol.h"

//utilities cribbed from powerslaves library
//https://github.com/kitling/powerslaves
namespace powerslaves
{
	enum CommandType
	{
		TEST = 0x02,
		SWITCH_MODE = 0x10,
		NTR_MODE = 0x11,
		NTR = 0x13,
		CTR = 0x14,
		SPI = 0x15
	};

	// Takes a buffer and reads len bytes into it.
	void readData(uint8_t *buf, unsigned len);

	// Takes a command type, command buffer with length, and the length of the expected response.
	bool sendMessage(enum CommandType type, const uint8_t *cmdbuf, uint8_t len, uint16_t response_len);

	//for reference
	//void simpleNTRcmd(uint8_t command, uint8_t *buf, unsigned len); // Convience function that takes a single byte for the command buffer and reads data into the buffer.
	//#define sendGenericMessage(type) sendMessage(type, NULL, 0x00, 0x00)
	//#define sendGenericMessageLen(type, response_length) sendMessage(type, NULL, 0x00, response_length)
	//#define sendNTRMessage(cmdbuf, response_length) sendMessage(NTR, cmdbuf, 0x08, response_length)
	//#define sendSPIMessage(cmdbuf, len, response_length) sendMessage(SPI, cmdbuf, len, response_length)
	//#define readHeader(buf, len) simpleNTRcmd(0x00, buf, len)
	//#define readChipID(buf) simpleNTRcmd(0x90, buf, 0x4)
	//#define dummyData(buf, len) simpleNTRcmd(0x9F, buf, len)

	static hid_device* getPowersaves() {
		static hid_device *device = NULL;
		if (!device) {
			device = hid_open(0x1C1A, 0x03D5, NULL);
			if (device == NULL) {
				struct hid_device_info *enumeration = hid_enumerate(0, 0);
				if (!enumeration) {
					printf("No HID devices found! Try running as root/admin?");
					return NULL;
				}
				free(enumeration);
				printf("No PowerSaves device found!");
				return NULL;
			}
		}

		return device;
	}


	void readData(uint8_t *buf, unsigned len) {
		if (!buf) return;
		unsigned iii = 0;
		while (iii < len) {
			iii += hid_read(getPowersaves(), buf + iii, len - iii);
			// printf("Bytes read: 0x%x\n", iii);
		}
	}

	bool sendMessage(CommandType type, const uint8_t *cmdbuf, uint8_t cmdlen, uint16_t response_len)
	{
		//apparently this should be this exact size, is it because of USB or HID or the powersaves device protocol?
		static const int kMsgbufSize = 65;
		u8 msgbuf[kMsgbufSize];

		//dont overflow this buffer (not sure how the size was picked, but I'm not questioning it now)
		if(cmdlen + 6 > kMsgbufSize) return false;
		memcpy(msgbuf + 6, cmdbuf, cmdlen);

		//fill powersaves protocol header:
		msgbuf[0] = 0; //reserved
		msgbuf[1] = type; //powersaves command type
		msgbuf[2] = cmdlen; //length of cmdbuf
		msgbuf[3] = 0x00; //reserved
		msgbuf[4] = (uint8_t)(response_len & 0xFF); //length of response (LSB)
		msgbuf[5] = (uint8_t)((response_len >> 8) & 0xFF); //length of response (MSB)

		hid_write(getPowersaves(), msgbuf, kMsgbufSize);
		return true;
	}

	static void cartInit()
	{
		sendMessage(SWITCH_MODE, NULL, 0, 0);
		sendMessage(NTR_MODE, NULL, 0, 0);
	
		//is this really needed?
		u8 junkbuf[0x40];
		sendMessage(TEST, NULL, 0, 0x40);
		readData(junkbuf, 0x40);
	}

} //namespace powerslaves

class Slot1_PowerSaves : public ISlot1Interface
{
private:
	u8 buf[0x2000+0x2000]; //0x2000 is largest, and 0x2000 for highest potential keylength
	u32 bufwords, bufidx;

public:
	Slot1_PowerSaves()
	{
	}

	virtual Slot1Info const* info()
	{
		static Slot1InfoSimple info("PowerSaves", "PowerSaves Card Reader", 0x05);
		return &info;
	}

	//called once when the emulator starts up, or when the device springs into existence
	virtual bool init()
	{
		return true;
	}
	
	virtual void connect()
	{
		powerslaves::cartInit();
		//
		//u8 junkbuf[0x2000];
		//powerslaves::simpleNTRcmd(0x9F,powerslaves::junkbuf, 0x2000); //needed?
	}

	//called when the emulator disconnects the device
	virtual void disconnect()
	{
	}
	
	//called when the emulator shuts down, or when the device disappears from existence
	virtual void shutdown()
	{ 
	}

	virtual void write_command(u8 PROCNUM, GC_Command command)
	{
		u16 reply = 0;
		
		u32 romctrl = T1ReadLong(MMU.MMU_MEM[PROCNUM][0x40], 0x1A4);
		u32 keylength = (romctrl&0x1FFF); //key1length high gcromctrl[21:16] ??
		u32 blocksize_field = (romctrl>>24)&7;
		static const u32 blocksize_table[] = {0,0x200,0x400,0x800,0x1000,0x2000,0x4000,4};
		u32 blocksize = blocksize_table[blocksize_field];

		reply = blocksize + keylength;

		command.print();
		printf("doing IO of %d\n",reply,command.bytes[0]);

		powerslaves::sendMessage(powerslaves::NTR, command.bytes, 8, reply);
		powerslaves::readData(buf,reply);
		bufidx = 0;
		bufwords = reply/4;

		if(command.bytes[0] == 0xB7)
		{
			int zzz=9;
		}
	}
	virtual void write_GCDATAIN(u8 PROCNUM, u32 val)
	{
		//powerslaves::simpleNTRcmd(0x9F,powerslaves::junkbuf, 0x2000); //needed?
		//powerslaves::simpleNTRcmd
	}
	virtual u32 read_GCDATAIN(u8 PROCNUM)
	{
		if(bufidx==bufwords) return 0; //buffer exhausted. is there additional stuff to emulate? we could read more from the card, and what does it return?
		return T1ReadLong(buf,(bufidx++)*4);
	}

	virtual void post_fakeboot(int PROCNUM)
	{
		//supporting this will be tricky!
		//we may need to send a complete reboot/handshake sequence to the powersaves device
	}

	void write32_GCDATAIN(u32 val)
	{
	}
};

ISlot1Interface* construct_Slot1_PowerSaves() { return new Slot1_PowerSaves(); }