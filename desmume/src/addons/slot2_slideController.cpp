//Serial handling code heavily inspired by the Magic Reader code in GBE+
//https://github.com/shonumi/gbe-plus/blob/7986317958a672d72ff54ea33f31bcca13cf8330/src/nds/slot2.cpp#L155

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

//Absolute barebones implementation of the Slide Controller add-on.

#include "../slot2.h"
#include "../emufile.h"

static u8 delta_x;
static u8 delta_y;
//Product ID, Revision ID, Motion status, X delta, Y delta, Surface quality
//Average pixel, Maximum pixel, Reserved, Reserved, Configuration, Reserved
//Data out lower, Data out upper, Shutter lower, Shutter upper, Frame period lower, Frame period upper
static u8 sc_regs[18] =
{ 0x03, 0x20, 0x00, 0x00, 0x00, 0x80,
  0x20, 0x3F, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x64, 0x00, 0x20, 0xD1
};

class Slot2_SlideController : public ISlot2Interface
{
private:
	u8 old_rumble;

	struct
	{
		u16 in_data;
		u16 out_data;
		u8 counter;
		u8 state;
		u8 sck;
		u8 reg_sel;
		u8 tmp;
	} slideCon = {};

	void slideCon_reset()
	{
		delta_x = 0;
		delta_y = 0;

		old_rumble = 0;
		if (FeedbackON)
			FeedbackON(false);

		slideCon.in_data = 0;
		slideCon.out_data = 0;
		slideCon.counter = 0;
		slideCon.state = 0;
		slideCon.sck = 0;
		slideCon.reg_sel = 0;
		slideCon.tmp = 0;

		sc_regs[0x02] = 0x00; //Motion status
		sc_regs[0x03] = 0x00; //X delta
		sc_regs[0x04] = 0x00; //Y delta
		sc_regs[0x0A] = 0x00; //Config bits
		sc_regs[0x10] = 0x20; //Frame period lower
		sc_regs[0x11] = 0xD1; //Frame period upper
	}

	void slideCon_process()
	{
		//Serial clock in bit 1
		u8 new_sck = (slideCon.in_data & 0x2) >> 1;
		//Serial data in bit 0
		u8 sd = slideCon.in_data & 0x1;
		//Rumble in bit 8
		u8 rumble = (slideCon.in_data & 0x100) >> 8;

		if (FeedbackON && (old_rumble != rumble))
		{
			old_rumble = rumble;
			FeedbackON(rumble);
		}

		switch (slideCon.state)
		{
			case 0: //Reg select
				//Build reg select byte
				if ((slideCon.sck == 0) && (new_sck == 1) && (slideCon.counter < 8))
				{
					slideCon.reg_sel = (slideCon.reg_sel << 1) | sd;
					slideCon.counter++;
				}
				else if (slideCon.counter == 8)
				{
					//Check if it's a read or a write by MSB
					if (slideCon.reg_sel & 0x80)
					{
						slideCon.state = 1;
						slideCon.reg_sel &= 0x7F;
						slideCon.tmp = 0;
					}
					else
					{
						slideCon.state = 2;

						if (slideCon.reg_sel == 0x02)
						{
							//Set motion flag if there has been movement
							if (delta_x || delta_y)
								sc_regs[0x02] |= 0x80;
							//Freeze motion deltas
							sc_regs[0x03] = delta_x;
							sc_regs[0x04] = delta_y;
							delta_x = delta_y = 0;
						}
					}
					slideCon.counter = 0;
				}
				break;
			case 1: //Write reg
				if ((slideCon.sck == 0) && (new_sck == 1) && (slideCon.counter < 8))
				{
					slideCon.tmp = (slideCon.tmp << 1) | sd;
					slideCon.counter++;
				}
				else if ((slideCon.sck == 0) && (new_sck == 0) && (slideCon.counter == 8))
				{
					//printf("SLIDECON WRITE REG: %02X = %02X\n", slideCon.reg_sel, slideCon.tmp);
					slideCon.state = 0;
					slideCon.counter = 0;

					if (slideCon.reg_sel <= 0x11)
						sc_regs[slideCon.reg_sel] = slideCon.tmp;
				}
				break;
			case 2: //Read reg
				if ((slideCon.sck == 0) && (new_sck == 1) && (slideCon.counter < 8))
				{
					if (slideCon.reg_sel <= 0x11)
						slideCon.out_data = (sc_regs[slideCon.reg_sel] >> (7 - slideCon.counter)) & 1;
					else
						slideCon.out_data = 0;
					slideCon.counter++;
				}
				else if ((slideCon.sck == 0) && (new_sck == 0) && (slideCon.counter == 8))
				{
					//printf("SLIDECON READ REG: %02X = %02X\n", slideCon.reg_sel, sc_regs[slideCon.reg_sel]);
					slideCon.state = 0;
					slideCon.counter = 0;

					//Reset motion flag if reg was motion status
					if (slideCon.reg_sel == 0x02)
						sc_regs[0x02] &= 0x7F;
					//Reset motion deltas if they were read
					if ((slideCon.reg_sel == 0x03) || (slideCon.reg_sel == 0x04))
						sc_regs[slideCon.reg_sel] = 0x00;
				}
				break;
		}

		slideCon.sck = new_sck;

		if (sc_regs[0x0A] & 0x80) //Reset
		{
			slideCon_reset();
		}
	}

public:

	virtual Slot2Info const* info()
	{
		static Slot2InfoSimple info("Slide Controller", "Slide Controller add-on", 0x0A);
		return &info;
	}

	virtual void connect()
	{
		slideCon_reset();
	}

	virtual void disconnect()
	{
		if (FeedbackON)
			FeedbackON(false);
	}

	virtual void writeWord(u8 PROCNUM, u32 addr, u16 val)
	{
		if (addr == 0x081E0000)
		{
			slideCon.in_data = val;
			slideCon_process();
		}
	}

	virtual u8	readByte(u8 PROCNUM, u32 addr)
	{
		if (addr == 0x080000B2)
			return 0x96;
		else
			return 0xFF;
	}

	virtual u16	readWord(u8 PROCNUM, u32 addr)
	{
		u16 outWord = 0xFFFF;

		if (addr < 0x08000100)
			outWord = 0xFEF0;
		else if (addr == 0x081E0000)
			outWord = slideCon.out_data;

		return outWord;
	}

	virtual u32 readLong(u8 PROCNUM, u32 addr) { return 0xFEF0FEF0; }

	virtual void savestate(EMUFILE& os)
	{
		s32 version = 0;
		os.write_32LE(version);

		os.write_u8(delta_x);
		os.write_u8(delta_y);
		for (int i = 0; i < 18; i++)
			os.write_u8(sc_regs[i]);
		os.write_16LE(slideCon.in_data);
		os.write_16LE(slideCon.out_data);
		os.write_u8(slideCon.counter);
		os.write_u8(slideCon.state);
		os.write_u8(slideCon.sck);
		os.write_u8(slideCon.reg_sel);
		os.write_u8(slideCon.tmp);
	}

	virtual void loadstate(EMUFILE& is)
	{
		s32 version = is.read_s32LE();

		if (version == 0)
		{
			is.read_u8(delta_x);
			is.read_u8(delta_y);
			for (int i = 0; i < 18; i++)
				sc_regs[i] = is.read_u8();
			is.read_16LE(slideCon.in_data);
			is.read_16LE(slideCon.out_data);
			is.read_u8(slideCon.counter);
			is.read_u8(slideCon.state);
			is.read_u8(slideCon.sck);
			is.read_u8(slideCon.reg_sel);
			is.read_u8(slideCon.tmp);
		}

		old_rumble = 0;
		if (FeedbackON)
			FeedbackON(false);
	}
};

ISlot2Interface* construct_Slot2_SlideController() { return new Slot2_SlideController(); }

void slideController_updateMotion(s8 x, s8 y)
{
	delta_x = (u8)x;
	delta_y = (u8)y;
}
