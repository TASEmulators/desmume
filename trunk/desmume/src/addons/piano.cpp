/*  Copyright (C) 2010 DeSmuME team

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "../addons.h"
#include <string.h>

static u16	pianoKeyStatus = 0;

static BOOL piano_init(void) { return (TRUE); }
static void piano_reset(void)
{
	//INFO("piano: Reset\n");
	pianoKeyStatus = 0;
}

static void piano_close(void) {}
static void piano_config(void) {}
static void piano_write08(u32 procnum, u32 adr, u8 val)
{
	//INFO("piano: write 08 at 0x%08X = %02X\n", adr, val);
}
static void piano_write16(u32 procnum, u32 adr, u16 val)
{
	//INFO("piano: write 16 at 0x%08X = %04X\n", adr, val);
}
static void piano_write32(u32 procnum, u32 adr, u32 val)
{
	//INFO("piano: write 32 at 0x%08X = %08X\n", adr, val);
}
extern int currFrameCounter;
static u8   piano_read08(u32 procnum, u32 adr)
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

	if(adr == 0x09FFFFFE) return ~(pianoKeyStatus&0xFF);
	if(adr == 0x09FFFFFF) return ~((pianoKeyStatus>>8)&0xFF);

	if(adr&1) return 0x07;
	else return 0x00;
}
static u16  piano_read16(u32 procnum, u32 adr)
{
	//printf("piano: read 16 at 0x%08X\n", adr);
	return 0x07FF;
}
static u32  piano_read32(u32 procnum, u32 adr)
{
	//printf("piano: read 32 at 0x%08X\n", adr);
	return 0x07FF07FF;
}
static void piano_info(char *info) { strcpy(info, "Piano for EasyPiano"); }

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

#define BIT(N,v) ((v)?(1<<(N)):0)
	pianoKeyStatus = 
		BIT(0,c) |
		BIT(1,cs) |
		BIT(2,d) |
		BIT(3,ds) |
		BIT(4,e) |
		BIT(5,f) |
		BIT(6,fs) |
		BIT(7,g) |
		BIT(8,gs) |
		BIT(9,a) |
		BIT(10,as) |
		BIT(13,b) |
		BIT(14,hic)
		;
	pianoKeyStatus = pianoKeyStatus;
}

ADDONINTERFACE addonPiano = {
	"Piano",
	piano_init,
	piano_reset,
	piano_close,
	piano_config,
	piano_write08,
	piano_write16,
	piano_write32,
	piano_read08,
	piano_read16,
	piano_read32,
	piano_info};
