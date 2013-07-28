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

#include "../slot1.h"
#include "../registers.h"
#include "../MMU.h"
#include "../NDSSystem.h"

static void info(char *info) { strcpy(info, "Slot1 Retail card emulation"); }
static void config(void) {}

static BOOL init() { return (TRUE); }

static void reset()
{
	// Write the header checksum to memory (the firmware needs it to see the cart)
#ifdef _NEW_BOOT
	if (!CommonSettings.BootFromFirmware)
#endif
	_MMU_write16<ARMCPU_ARM9>(0x027FF808, T1ReadWord(MMU.CART_ROM, 0x15E));
}

static void close() {}


static void write08(u8 PROCNUM, u32 adr, u8 val) {}
static void write16(u8 PROCNUM, u32 adr, u16 val) {}

static void write32_GCROMCTRL(u8 PROCNUM, u32 val)
{
	nds_dscard& card = MMU.dscard[PROCNUM];

	if (card.mode == CardMode_Normal)
	{
		switch(card.command[0])
		{
			case 0x00: //Data read (0000000000000000h Get Cartridge Header) - len 200h bytes
			case 0xB7:
				card.address = 	(card.command[1] << 24) | (card.command[2] << 16) | (card.command[3] << 8) | card.command[4];
				card.transfer_count = 0x80;
				break;

			case 0x90:	// 1st Get ROM Chip ID - len 4 bytes
			case 0xB8:
				card.address = 0;
				card.transfer_count = 1;
				break;

			default:
				card.address = 0;
				card.transfer_count = 0;
				printf("ARM%c: SLOT1 invalid command %02X (write) - CardMode_Normal\n", PROCNUM?'7':'9', card.command[0]);
				break;
		}
		return;
	}

	if (card.mode == CardMode_KEY1 || card.mode == CardMode_KEY2)
	{
		u8 cmd = (card.command[0] >> 4);
		switch(cmd)
		{
			case 0x01:	// 2nd Get ROM Chip ID - len 4 bytes
				card.address = 0;
				card.transfer_count = 1;
			break;

			default:
				card.address = 0;
				card.transfer_count = 0;
				printf("ARM%c: SLOT1 invalid command %02X (write) - %s\n", PROCNUM?'7':'9', cmd, (card.mode == CardMode_KEY1)?"CardMode_KEY1":"CardMode_KEY2");
			break;
		}
		return;
	}

	if (card.mode == CardMode_DATA_LOAD)
	{
		switch(card.command[0])
		{
			case 0xB7:	// Encrypted Data Read (B7aaaaaaaa000000h) - len 200h bytes
				card.address = 	(card.command[1] << 24) | (card.command[2] << 16) | (card.command[3] << 8) | card.command[4];
				card.transfer_count = 0x80;
			break;

			case 0xB8:	// 3nd Get ROM Chip ID - len 4 bytes
				card.address = 0;
				card.transfer_count = 1;
			break;

			default:
				card.address = 0;
				card.transfer_count = 0;
				card.mode = CardMode_KEY1;
				printf("ARM%c: SLOT1 invalid command %02X (write) - CardMode_DATA_LOAD\n", PROCNUM?'7':'9', card.command[0]);
			break;
		}
		return;
	}
}

static void write32(u8 PROCNUM, u32 adr, u32 val)
{
	switch(adr)
	{
	case REG_GCROMCTRL:
		write32_GCROMCTRL(PROCNUM, val);
		break;
	}
}

static u8 read08(u8 PROCNUM, u32 adr)
{
	return 0xFF;
}
static u16 read16(u8 PROCNUM, u32 adr)
{
	return 0xFFFF;
}

static u32 read32_GCDATAIN(u8 PROCNUM)
{
	nds_dscard& card = MMU.dscard[PROCNUM];
	u8 cmd = card.command[0];

	if (card.mode == CardMode_KEY1 || card.mode == CardMode_KEY2)
		cmd >>= 4;

	switch(cmd)
	{
		case 0x01:	// 2nd Get ROM Chip ID - len 4 bytes
		case 0x90:	// 1st Get ROM Chip ID - len 4 bytes
		case 0xB8:	// 3rd Get ROM Chip ID - len 4 bytes
			{
				// Returns RAW unencrypted Chip ID (eg. C2h,0Fh,00h,00h), repeated every 4 bytes.
				//
				// 1st byte - Manufacturer (C2h = Macronix)
				// 2nd byte - Chip size in megabytes minus 1 (eg. 0Fh = 16MB)
				// 3rd byte - Reserved/zero (probably upper bits of chip size)
				// 4th byte - Bit7: Secure Area Block transfer mode (8x200h or 1000h)

#ifdef _NEW_BOOT
				u32 chipID = 0;
				if (CommonSettings.BootFromFirmware)
					chipID = 0x00000000 | 0x00000000 | 0x00000F00 | 0x000000C2;;
#else
				u32 chipID = 0;
#endif
 
				// Note: the BIOS stores the chip ID in main memory
				// Most games continuously compare the chip ID with
				// the value in memory, probably to know if the card
				// was removed.
				// As DeSmuME normally boots directly from the game, the chip
				// ID in main mem is zero and this value needs to be
				// zero too.

				//note that even if desmume was booting from firmware, and reading this chip ID to store in main memory,
				//this still works, since it will have read 00 originally and then read 00 to validate.

				//staff of kings verifies this (it also uses the arm7 IRQ 20)
				if(nds.cardEjected) //TODO - handle this with ejected card slot1 device (and verify using this case)
					return 0xFFFFFFFF;
				else 
					return chipID;
			}
			break;


		// Data read
		case 0x00:
		case 0xB7:
			{
				//it seems that etrian odyssey 3 doesnt work unless we mask this to cart size.
				//but, a thought: does the internal rom address counter register wrap around? we may be making a mistake by keeping the extra precision
				//but there is no test case yet
				u32 address = card.address & (gameInfo.mask);

				// Make sure any reads below 0x8000 redirect to 0x8000+(adr&0x1FF) as on real cart
				if((cmd == 0xB7) && (address < 0x8000))
				{
					//TODO - refactor this to include the PROCNUM, for debugging purposes if nothing else
					//(can refactor gbaslot also)

					//INFO("Read below 0x8000 (0x%04X) from: ARM%s %08X\n",
					//	card.address, (PROCNUM ? "7":"9"), (PROCNUM ? NDS_ARM7:NDS_ARM9).instruct_adr);

					address = (0x8000 + (address&0x1FF));
				}

				//as a sanity measure for funny-sized roms (homebrew and perhaps truncated retail roms)
				//we need to protect ourselves by returning 0xFF for things still out of range
				if(address >= gameInfo.romsize)
				{
					DEBUG_Notify.ReadBeyondEndOfCart(address,gameInfo.romsize);
					return 0xFFFFFFFF;
				}

				return T1ReadLong(MMU.CART_ROM, address);
			}
			break;
		default:
#ifdef _NEW_BOOT
			printf("ARM%c: SLOT1 invalid command %02X (read)\n", PROCNUM?'7':'9', cmd);
#endif
			return 0;
	} //switch(card.command[0])
} //read32_GCDATAIN

static u32 read32(u8 PROCNUM, u32 adr)
{
	switch(adr)
	{
	case REG_GCDATAIN:
		return read32_GCDATAIN(PROCNUM);
	default:
		return 0;
	}
}


SLOT1INTERFACE slot1Retail = {
	"Retail",
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


