/*
	Copyright (C) 2010-2013 DeSmuME team

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

static u16	pianoKeyStatus = 0;

class Slot2_EasyPiano : public ISlot2Interface
{
public:
	virtual Slot2Info const* info()
	{
		static Slot2InfoSimple info("Piano", "Piano for EasyPiano", 0x06);
		return &info;
	}

	virtual void connect()
	{
		pianoKeyStatus = 0;
	}

	virtual u8	readByte(u8 PROCNUM, u32 addr)
	{
		//printf("piano: read 08 at 0x%08X\n", adr);

		//the actual keyboard output

		//byte:bit
		//0x09FFFFFE:0 = C
		//0x09FFFFFE:1 = C#
		//0x09FFFFFE:2 = D
		//0x09FFFFFE:3 = D#
		//0x09FFFFFE:4 = E
		//0x09FFFFFE:5 = F
		//0x09FFFFFE:6 = F#
		//0x09FFFFFE:7 = G
		//0x09FFFFFF:0 = G#
		//0x09FFFFFF:1 = A
		//0x09FFFFFF:2 = A#
		//0x09FFFFFF:3 = ?
		//0x09FFFFFF:4 = ?
		//0x09FFFFFF:5 = B
		//0x09FFFFFF:6 = hiC
		//0x09FFFFFF:7 = ?

		//deassert bit if key is pressed

		//LOG("PIANO: %04X\n",pianoKeyStatus);

		if(addr == 0x09FFFFFE) return (~(pianoKeyStatus&0xFF));
		if(addr == 0x09FFFFFF) return (~((pianoKeyStatus>>8)&0xFF))&~(0x18);

		return (addr & 1)?0xE7:0xFF;
	}
	virtual u16	readWord(u8 PROCNUM, u32 addr)
	{
		if (addr != 0x09FFFFFE)
			return 0xE7FF;
		
		return readByte(PROCNUM, 0x09FFFFFE) | (readByte(PROCNUM,0x09FFFFFF) << 8);
	}
	virtual u32	readLong(u8 PROCNUM, u32 addr) { return 0xE7FFE7FF; }

};

ISlot2Interface* construct_Slot2_EasyPiano() { return new Slot2_EasyPiano(); }

void piano_setKey(bool c, bool cs, bool d, bool ds, bool e, bool f, bool fs, bool g, bool gs, bool a, bool as, bool b, bool hic)
{
	//0x09FFFFFE:0 = C
	//0x09FFFFFE:1 = C#
	//0x09FFFFFE:2 = D
	//0x09FFFFFE:3 = D#
	//0x09FFFFFE:4 = E
	//0x09FFFFFE:5 = F
	//0x09FFFFFE:6 = F#
	//0x09FFFFFE:7 = G
	//0x09FFFFFE:0 = G#
	//0x09FFFFFE:1 = A
	//0x09FFFFFF:2 = A#
	//0x09FFFFFF:3 = ?
	//0x09FFFFFF:4 = ?
	//0x09FFFFFF:5 = B
	//0x09FFFFFF:6 = hiC
	//0x09FFFFFF:7 = ?

	#define BIT_P(N,v) ((v)?(1<<(N)):0)
		pianoKeyStatus = 
			BIT_P(0,c) |
			BIT_P(1,cs) |
			BIT_P(2,d) |
			BIT_P(3,ds) |
			BIT_P(4,e) |
			BIT_P(5,f) |
			BIT_P(6,fs) |
			BIT_P(7,g) |
			BIT_P(8,gs) |
			BIT_P(9,a) |
			BIT_P(10,as) |
			BIT_P(13,b) |
			BIT_P(14,hic)
			;
}
