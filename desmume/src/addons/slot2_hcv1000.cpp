//HCV-1000 emulation code adapted from GBE+: https://github.com/shonumi/gbe-plus

/*
	Modifications Copyright (C) 2023 DeSmuME team

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

#include <string.h>

#include "../slot2.h"

u8 hcv1000_cnt;
char hcv1000_data[16];

class Slot2_HCV1000 : public ISlot2Interface
{
public:

	virtual Slot2Info const* info()
	{
		static Slot2InfoSimple info("Sega Card Reader", "Sega Card Reader(HCV-1000) add-on", 0x09);
		return &info;
	}

	virtual bool init()
	{
		hcv1000_cnt = 0;
		memset(hcv1000_data, 0x5F, 16);

		return TRUE;
	}

	virtual void writeByte(u8 PROCNUM, u32 addr, u8 val)
	{
		if (addr == 0xA000000) { hcv1000_cnt = (val & 0x83); }
	}

	virtual u8	readByte(u8 PROCNUM, u32 addr)
	{
		u8 slot_byte = 0xFF;
		//Reading these cart addresses is for detection
		if (addr < 0x8020000)
		{
			u8 data = 0xF0 | ((addr & 0x1F) >> 1);
			slot_byte = (addr & 0x1) ? 0xFD : data;
		}

		//HCV_CNT
		else if (addr == 0xA000000) { slot_byte = hcv1000_cnt; }

		//HCV_DATA
		else if ((addr >= 0xA000010) && (addr <= 0xA00001F))
		{
			slot_byte = (u8)hcv1000_data[addr & 0xF];
		}

		return slot_byte;
	}

	virtual u16	readWord(u8 PROCNUM, u32 addr) { return 0xFDFD; };
	virtual u32	readLong(u8 PROCNUM, u32 addr) { return 0xFDFDFDFD; };
};

ISlot2Interface* construct_Slot2_HCV1000() { return new Slot2_HCV1000(); }

void HCV1000_setReady()
{
	hcv1000_cnt &= ~0x80;
}

void HCV1000_setBarcode(std::string barcode)
{
	barcode.resize(16, '_');
	memcpy(hcv1000_data, barcode.c_str(), barcode.length());
}
