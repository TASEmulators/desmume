/*
	Copyright (C) 2023 DeSmuME team

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

//Absolute barebones implementation of the Slide Controller add-on. Rumble is not implemented.
//The game does a bunch of mystery reads of various sizes which have not been investigated at all.

#include "../slot2.h"
#include "../emufile.h"

#define SC_BITIN(X, Y)		\
do {						\
	X = (X << 1) | (Y & 1); \
	bitsWritten++;			\
} while (0)

#define SC_BITOUT(X, Y)				\
do {								\
	X = ((Y >> 7 - bitsRead) & 1);	\
	bitsRead++;						\
} while(0)

u8 motionStatus;
u8 xMotion;
u8 yMotion;

class Slot2_SlideController : public ISlot2Interface
{
private:
	u8 prevClock;
	u8 bitsWritten;
	u8 bitsRead;
	u8 inByte;
	u16 regSel;

	u8 configBits;
	u8 framePeriodLower;
	u8 framePeriodUpper;
public:

	virtual Slot2Info const* info()
	{
		static Slot2InfoSimple info("Slide Controller", "Slide Controller add-on", 0x0A);
		return &info;
	}

	virtual void connect()
	{
		prevClock = 0;
		bitsWritten = 0;
		bitsRead = 0;
		inByte = 0;
		regSel = 0xFFFF;

		motionStatus = 0;
		xMotion = 0;
		yMotion = 0;
		configBits = 0;
		framePeriodLower = 0x20;
		framePeriodUpper = 0xD1;
	}

	virtual void writeWord(u8 PROCNUM, u32 addr, u16 val)
	{
		if (addr == 0x081E0000)
		{
			if ((prevClock == 0) && (val & 0x2) && (val & 0x8))
			{
				SC_BITIN(inByte, val);
				if (bitsWritten == 8)
				{
					bitsWritten = 0;
					if (regSel == 0xFFFF) //First byte written
					{
						regSel = inByte;
					}
					else //Second byte written
					{
						//printf("WRITE TO REG: %02X = %02X\n", regSel & 0x7F , inByte);
						switch (regSel & 0x7F)
						{
							case 0x0A: //Config bits
								configBits = inByte;
								break;
							case 0x10: //Frame period (lower)
								framePeriodLower = inByte;
								break;
							case 0x11: //Frame period(upper)
								framePeriodUpper = inByte;
								break;
						}
						regSel = 0xFFFF;
					}
				}
			}
			prevClock = (val & 0x2) >> 1;
		}

		if (configBits & 0x80) //Reset
		{
			motionStatus = 0;
			xMotion = 0;
			yMotion = 0;
			configBits = 0;
			framePeriodLower = 0x20;
			framePeriodUpper = 0xD1;

			prevClock = 0;
			bitsWritten = 0;
			bitsRead = 0;
			inByte = 0;
			regSel = 0xFFFF;
		}
	}

	virtual u8	readByte(u8 PROCNUM, u32 addr)
	{
		if (addr == 0x080000B2)
		{
			return 0x96;
		}
		return 0xFF;
	}

	virtual u16	readWord(u8 PROCNUM, u32 addr)
	{
		u16 outWord = 0;

		if (addr < 0x08000100)
		{
			outWord = 0xFEF0;
		}

		if (addr == 0x081E0000)
		{
			switch (regSel)
			{
				case 0x00: //Product ID
					SC_BITOUT(outWord, 0x3);
					break;
				case 0x01: //Revision ID
					SC_BITOUT(outWord, 0x20); //Is this correct?
					break;
				case 0x02: //Motion status
					SC_BITOUT(outWord, motionStatus);
					if (bitsRead == 8) motionStatus = 0;
					break;
				case 0x03: //X motion delta
					SC_BITOUT(outWord, xMotion);
					if (bitsRead == 8) xMotion = 0;
					break;
				case 0x04: //Y motion delta
					SC_BITOUT(outWord, yMotion);
					if (bitsRead == 8) yMotion = 0;
					break;
				case 0x05: //Surface quality
					SC_BITOUT(outWord, 128);
					break;
				case 0x06: //Average pixel
					SC_BITOUT(outWord, 32);
					break;
				case 0x07: //Maximum pixel
					SC_BITOUT(outWord, 63);
					break;
				case 0x0A: //Configuration bits
					SC_BITOUT(outWord, configBits);
					break;
				case 0x0C: //Data out (lower)
					SC_BITOUT(outWord, 0);
					break;
				case 0x0D: //Data out (upper)
					SC_BITOUT(outWord, 0);
					break;
				case 0x0E: //Shutter value (lower)
					SC_BITOUT(outWord, 64);
					break;
				case 0x0F: //Shutter value (upper)
					SC_BITOUT(outWord, 0);
					break;
				case 0x10: //Frame period (lower)
					SC_BITOUT(outWord, framePeriodLower);
					break;
				case 0x11: //Frame period (upper)
					SC_BITOUT(outWord, framePeriodUpper);
					break;
			}

			if (bitsRead == 8)
			{
				bitsRead = 0;
				//printf("READ FROM REG: %02X\n", regSel);
				regSel = 0xFFFF;
			}
		}

		return outWord;
	}

	virtual void savestate(EMUFILE& os)
	{
		s32 version = 0;
		os.write_32LE(version);

		os.write_u8(prevClock);
		os.write_u8(bitsWritten);
		os.write_u8(bitsRead);
		os.write_u8(inByte);
		os.write_16LE(regSel);

		os.write_u8(configBits);
		os.write_u8(framePeriodLower);
		os.write_u8(framePeriodUpper);
		os.write_u8(motionStatus);
		os.write_u8(xMotion);
		os.write_u8(yMotion);
	}

	virtual void loadstate(EMUFILE& is)
	{
		s32 version = is.read_s32LE();

		if (version == 0)
		{
			is.read_u8(prevClock);
			is.read_u8(bitsWritten);
			is.read_u8(bitsRead);
			is.read_u8(inByte);
			is.read_16LE(regSel);

			is.read_u8(configBits);
			is.read_u8(framePeriodLower);
			is.read_u8(framePeriodUpper);
			is.read_u8(motionStatus);
			is.read_u8(xMotion);
			is.read_u8(yMotion);
		}
	}
};

ISlot2Interface* construct_Slot2_SlideController() { return new Slot2_SlideController(); }

void slideController_updateMotion(s8 x, s8 y)
{
	xMotion = (u8)x;
	yMotion = (u8)y;
	if (xMotion || yMotion)
		motionStatus = 0x80;
}

#undef SC_BITIN
#undef SC_BITOUT
