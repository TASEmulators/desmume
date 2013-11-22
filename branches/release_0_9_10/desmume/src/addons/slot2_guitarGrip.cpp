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
static u8 guitarKeyStatus;

class Slot2_GuitarGrip : public ISlot2Interface
{
private: 
	
public:
	virtual Slot2Info const* info()
	{
		static Slot2InfoSimple info("Guitar Grip", "Guitar Grip for Guitar Hero games", 0x04);
		return &info;
	}

	virtual void connect()
	{
		guitarKeyStatus = 0;
	}

	virtual u8	readByte(u8 PROCNUM, u32 addr) 
	{
		if (addr == 0x0A000000) return (~guitarKeyStatus);
		return (addr & 1)?0xF9:0xFF; 
	}
	virtual u16	readWord(u8 PROCNUM, u32 addr) { return 0xF9FF; }
	virtual u32	readLong(u8 PROCNUM, u32 addr) { return 0xF9FFF9FF; }
};

ISlot2Interface* construct_Slot2_GuitarGrip() { return new Slot2_GuitarGrip(); }

void guitarGrip_setKey(bool green, bool red, bool yellow, bool blue)
{
	guitarKeyStatus = 0 | (green << 6) | (red << 5) | (yellow << 4) | (blue << 3);
}
