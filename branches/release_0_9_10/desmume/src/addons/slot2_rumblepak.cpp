/*
	Copyright (C) 2009 CrazyMax
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

#include "../slot2.h"

void (*FeedbackON)(BOOL enable) = NULL;

class Slot2_RumblePak : public ISlot2Interface
{
private:
	u16 old_val_rumble;

public:
	virtual Slot2Info const* info()
	{
		static Slot2InfoSimple info("Rumble Pak", "NDS Rumble Pak (need joystick with Feedback)", 0x02);
		return &info;
	}

	virtual void connect()
	{
		old_val_rumble = 0;
		if (!FeedbackON) return;
		FeedbackON(false);
	}

	virtual void writeWord(u8 PROCNUM, u32 addr, u16 val)
	{
		if (!FeedbackON) return;
		if (old_val_rumble == val) return;

		old_val_rumble = val;
		if ((addr == 0x08000000) || (addr == 0x08001000))
			FeedbackON(val);
	}

	virtual u8	readByte(u8 PROCNUM, u32 addr) { return (addr & 1)?0xFF:0xFD; };
	virtual u16	readWord(u8 PROCNUM, u32 addr) { return 0xFFFD; };
	virtual u32	readLong(u8 PROCNUM, u32 addr) { return 0xFFFDFFFD; };
};

ISlot2Interface* construct_Slot2_RumblePak() { return new Slot2_RumblePak(); }