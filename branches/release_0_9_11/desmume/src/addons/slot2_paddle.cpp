/*
	Copyright (C) 2011-2015 DeSmuME team

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
this device seems to have a 12bit value (like 20.12 fixed point.. get it?) located at 0x0A000000
clockwise = right = increases.
it returns the correct little endian bytes through byte reads.
through halfword reads, it returns the LSB twice
through full word reads, it returns the LSB four times. 
after that everything is 0x00.
if the slot2 timings are wrong, then this device will return glitchy or 0xFF output.
so this emulation code will attempt to validate that.
arkanoid was setting REG_EXMEMCNT = 0x082F

the rom returns 0xEFFF for all addresses and should be used to detect this device

writing any byte to SRAM or writing any halfword/word to rom results in a reset or some kind of recalibrate
the resulting value may be 0x000,0x001, or 0xFFF. seems mostly random, though once it is reset, resetting again won't change it.
you must wait until the paddle has been moved.

conclusion:
The emulation in all the handling of erroneous cases is not perfect, and some other users of the paddle (do any other games use it?)
maybe legally configure the paddle differently, which could be rejected here; in which case this code will need finetuning
*/

#include <string.h>

#include "../slot2.h"
#include "../NDSSystem.h"

class Slot2_Paddle : public ISlot2Interface
{
private:
	void calibrate() { nds.paddle = 0; }
	bool Validate(u32 procnum, bool rom)
	{
		if(rom)
			return ValidateSlot2Access(procnum, 0, 0, 0, -1);
		else
			return ValidateSlot2Access(procnum, 18, 0, 0, 1);
	}

public:
	virtual Slot2Info const* info()
	{
		static Slot2InfoSimple info("Paddle Controller", "Taito Paddle Controller", 0x07);
		return &info;
	}

	virtual void writeByte(u8 PROCNUM, u32 addr, u8 val)
	{
		if (addr < 0x0A000000) 
			calibrate();
	}
	virtual void writeWord(u8 PROCNUM, u32 addr, u16 val)
	{
		if (addr < 0x0A000000)
			calibrate();
	}
	virtual void writeLong(u8 PROCNUM, u32 addr, u32 val)
	{
		if (addr < 0x0A000000)
			calibrate();
	}

	virtual u8	readByte(u8 PROCNUM, u32 addr)
	{
		//printf("paddle: read 08 at 0x%08X\n", addr);
		if (!Validate(PROCNUM, (addr < 0x0A000000)))
			return 0xFF;

		if (addr < 0x0A000000)
			return (addr & 1)?0xFF:0xEF;

		if (addr == 0x0A000000)
			return (nds.paddle & 0xFF);

		if (addr == 0x0A000001)
			return ((nds.paddle >> 8) & 0x0F);

		return 0x00;
	}
	virtual u16	readWord(u8 PROCNUM, u32 addr)
	{
		//printf("paddle: read 16 at 0x%08X\n", addr);
		if (!Validate(PROCNUM, (addr < 0x0A000000)))
			return 0xFFFF;

		if (addr < 0x0A000000)
			return 0xEFFF;

		if (addr == 0x0A000000)
		{
			u8 val = (nds.paddle & 0xFF);
			return (val | (val << 8));
		}
		
		return 0x0000;
	}
	virtual u32	readLong(u8 PROCNUM, u32 addr)
	{
		//printf("paddle: read 32 at 0x%08X\n", addr);
		if (!Validate(PROCNUM, (addr < 0x0A000000)))
			return 0xFFFFFFFF;

		if (addr < 0x0A000000)
			return 0xEFFFEFFF;
		
		if (addr == 0x0A000000)
		{
			u8 val = (nds.paddle & 0xFF);
			return (val | (val << 8) | (val << 16) | (val << 24));
		}

		return 0x00000000;
	}
};

u16 Paddle_GetValue()
{
	return nds.paddle;
}

void Paddle_SetValue(u16 theValue)
{
	nds.paddle = theValue;
}

ISlot2Interface* construct_Slot2_Paddle() { return new Slot2_Paddle(); }
