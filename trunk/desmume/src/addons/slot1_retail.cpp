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
#include "../registers.h"
#include "../mmu.h"
#include "../NDSSystem.h"

static void info(char *info) { strcpy(info, "Slot1 Retail card emulation"); }
static void config(void) {}

static BOOL init() { return (TRUE); }

static void reset() {}

static void close() {}


static void write08(u32 adr, u8 val) {}
static void write16(u32 adr, u16 val) {}

static void write32_GCROMCTRL(u32 val)
{
	nds_dscard& card = MMU.dscard[0];

	switch(card.command[0])
	{
		case 0x00: //Data read
		case 0xB7:
			card.address = 	(card.command[1] << 24) | (card.command[2] << 16) | (card.command[3] << 8) | card.command[4];
			card.transfer_count = 0x80;
			break;
		default:
			card.address = 0;
			break;
	}
}

static void write32(u32 adr, u32 val)
{
	switch(adr)
	{
	case REG_GCROMCTRL:
		write32_GCROMCTRL(val);
		break;
	}
}

static u8 read08(u32 adr)
{
	return 0xFF;
}
static u16 read16(u32 adr)
{
	return 0xFFFF;
}

static u32 read32_GCDATAIN()
{
	nds_dscard& card = MMU.dscard[0];

	switch(card.command[0])
	{
		//Get ROM chip ID
		case 0x90:
		case 0xB8:
			{
				// Note: the BIOS stores the chip ID in main memory
				// Most games continuously compare the chip ID with
				// the value in memory, probably to know if the card
				// was removed.
				// As DeSmuME boots directly from the game, the chip
				// ID in main mem is zero and this value needs to be
				// zero too.

				//staff of kings verifies this (it also uses the arm7 IRQ 20)
				if(nds.cardEjected) //TODO - handle this with ejected card slot1 device (and verify using this case)
					return 0xFFFFFFFF;
				else return 0;
			}
			break;


		// Data read
		case 0x00:
		case 0xB7:
			{
				// Make sure any reads below 0x8000 redirect to 0x8000+(adr&0x1FF) as on real cart
				if((card.command[0] == 0xB7) && (card.address < 0x8000))
				{
					//TODO - refactor this to include the PROCNUM, for debugging purposes if nothing else
					//(can refactor gbaslot also)

					//INFO("Read below 0x8000 (0x%04X) from: ARM%s %08X\n",
					//	card.address, (PROCNUM ? "7":"9"), (PROCNUM ? NDS_ARM7:NDS_ARM9).instruct_adr);

					card.address = (0x8000 + (card.address&0x1FF));
				}

				if(card.address >= gameInfo.romsize)
				{
					DEBUG_Notify.ReadBeyondEndOfCart(card.address,gameInfo.romsize);
					return 0xFFFFFFFF;
				}
				else
				//but, this is actually handled by the cart rom buffer being oversized and full of 0xFF.
				//is this a good idea? We think so.

				return T1ReadLong(MMU.CART_ROM, card.address & MMU.CART_ROM_MASK);
			}
			break;
		default:
			return 0;
	} //switch(card.command[0])
} //read32_GCDATAIN

static u32 read32(u32 adr)
{
	switch(adr)
	{
	case REG_GCDATAIN:
		return read32_GCDATAIN();
	default:
		return 0;
	}
}


ADDONINTERFACE slot1Retail = {
	"Slot1Retail",
	init,
	reset,
	close,
	config,
	write08,
	write16,
	write32,
	read08,
	read16,
	read32,
	info
};



	//		///writetoGCControl:
	//// --- Ninja SD commands -------------------------------------

	//	// NJSD init/reset
	//	case 0x20:
	//		{
	//			card.address = 0;
	//			card.transfer_count = 0;
	//		}
	//		break;

	//	// NJSD_sendCLK()
	//	case 0xE0:
	//		{
	//			card.address = 0;
	//			card.transfer_count = 0;
	//			NDS_makeInt(PROCNUM, 20);
	//		}
	//		break;

	//	// NJSD_sendCMDN() / NJSD_sendCMDR()
	//	case 0xF0:
	//	case 0xF1:
	//		switch (card.command[2])
	//		{
	//		// GO_IDLE_STATE
	//		case 0x40:
	//			card.address = 0;
	//			card.transfer_count = 0;
	//			NDS_makeInt(PROCNUM, 20);
	//			break;

	//		case 0x42:  // ALL_SEND_CID
	//		case 0x43:  // SEND_RELATIVE_ADDR
	//		case 0x47:  // SELECT_CARD
	//		case 0x49:  // SEND_CSD
	//		case 0x4D:
	//		case 0x77:  // APP_CMD
	//		case 0x69:  // SD_APP_OP_COND
	//			card.address = 0;
	//			card.transfer_count = 6;
	//			NDS_makeInt(PROCNUM, 20);
	//			break;

	//		// SET_BLOCKLEN
	//		case 0x50:
	//			card.address = 0;
	//			card.transfer_count = 6;
	//			card.blocklen = card.command[6] | (card.command[5] << 8) | (card.command[4] << 16) | (card.command[3] << 24);
	//			NDS_makeInt(PROCNUM, 20);
	//			break;

	//		// READ_SINGLE_BLOCK
	//		case 0x51:
	//			card.address = card.command[6] | (card.command[5] << 8) | (card.command[4] << 16) | (card.command[3] << 24);
	//			card.transfer_count = (card.blocklen + 3) >> 2;
	//			NDS_makeInt(PROCNUM, 20);
	//			break;
	//		}
	//		break;

	//	// --- Ninja SD commands end ---------------------------------



	//		//GCDATAIN:
	//	// --- Ninja SD commands -------------------------------------

	//	// NJSD_sendCMDN() / NJSD_sendCMDR()
	//	case 0xF0:
	//	case 0xF1:
	//		switch (card.command[2])
	//		{
	//		// ALL_SEND_CID
	//		case 0x42:
	//			if (card.transfer_count == 2) val = 0x44534A4E;
	//			else val = 0x00000000;

	//		// SEND_RELATIVE_ADDR
	//		case 0x43:
	//		case 0x47:
	//		case 0x49:
	//		case 0x50:
	//			val = 0x00000000;
	//			break;

	//		case 0x4D:
	//			if (card.transfer_count == 2) val = 0x09000000;
	//			else val = 0x00000000;
	//			break;

	//		// APP_CMD
	//		case 0x77:
	//			if (card.transfer_count == 2) val = 0x00000037;
	//			else val = 0x00000000;
	//			break;

	//		// SD_APP_OP_COND
	//		case 0x69:
	//			if (card.transfer_count == 2) val = 0x00008000;
	//			else val = 0x00000000;
	//			break;

	//		// READ_SINGLE_BLOCK
	//		case 0x51:
	//			val = 0x00000000;
	//			break;
	//		}
	//		break;

	//	// --- Ninja SD commands end ---------------------------------


