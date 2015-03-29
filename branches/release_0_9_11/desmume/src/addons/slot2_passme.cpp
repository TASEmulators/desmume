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

#include "../slot2.h"
#include "../NDSSystem.h"

class Slot2_PassME : public ISlot2Interface
{
public:
	virtual Slot2Info const* info()
	{
		static Slot2InfoSimple info("PassME", "PassME in GBA slot", 0x08);
		return &info;
	}

	virtual u8	readByte(u8 PROCNUM, u32 addr)
	{
		u32 tmp_addr = (addr & 0x07FFFFFF);
		if (tmp_addr < gameInfo.romsize)
			return (u8)gameInfo.readROM(tmp_addr);

		return (0xFF);
	}
	virtual u16	readWord(u8 PROCNUM, u32 addr)
	{
		u32 tmp_addr = (addr & 0x07FFFFFF);
		if (tmp_addr < gameInfo.romsize)
			return (u16)gameInfo.readROM(tmp_addr);

		return (0xFFFF);
	}
	virtual u32	readLong(u8 PROCNUM, u32 addr)
	{
		u32 tmp_addr = (addr & 0x07FFFFFF);
		if (tmp_addr < gameInfo.romsize)
			return (u32)gameInfo.readROM(tmp_addr);

		return (0xFFFFFFFF);
	}
};

ISlot2Interface* construct_Slot2_PassME() { return new Slot2_PassME(); }
