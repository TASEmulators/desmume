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

// Games with NAND Flash:
// -  Ore/WarioWare D.I.Y.			- 128Mbit
// -  Daigassou! Band Brothers DX	- 64MBit (NAND?)

// Ore/WarioWare D.I.Y. - chip:		SAMSUNG 004
//									KLC2811ANB-P204
//									NTR-UORE-0

#include "../slot1.h"
#include "../registers.h"
#include "../MMU.h"
#include "../NDSSystem.h"

class Slot1_Retail_NAND : public ISlot1Interface
{
public:
	virtual Slot1Info const* info()
	{
		static Slot1InfoSimple info("Retail NAND","Slot1 retail NAND card emulation");
		return &info;
	}

	virtual void connect()
	{
	}

	virtual u32 read32(u8 PROCNUM, u32 adr)
	{
		switch(adr)
		{
		case REG_GCDATAIN:
			return read32_GCDATAIN(PROCNUM);
		default:
			return 0;
		}
	}

	virtual void write32(u8 PROCNUM, u32 adr, u32 val)
	{
		switch(adr)
		{
		case REG_GCROMCTRL:
			write32_GCROMCTRL(PROCNUM, val);
			break;
		}
	}


private:


	void write32_GCROMCTRL(u8 PROCNUM, u32 val)
	{
		nds_dscard& card = MMU.dscard[PROCNUM];

		switch(card.command[0])
		{
			case 0x00: //Data read
			case 0xB7:
				card.address = 	(card.command[1] << 24) | (card.command[2] << 16) | (card.command[3] << 8) | card.command[4];
				card.transfer_count = 0x200;
				break;

			case 0xB8:	// Chip ID
				card.address = 0;
				card.transfer_count = 4;
				break;

			// Nand Init
			case 0x94:
				card.address = 0;
				card.transfer_count = 0x200;
				break;

			// Nand Error?
			case 0xD6:
				card.address = 0;
				card.transfer_count = 4;
				break;
			
			// Nand Write? ---- PROGRAM for INTERNAL DATA MOVE/RANDOM DATA INPUT
			//case 0x8B:
			case 0x85:
				card.address = 0;
				card.transfer_count = 0x200;
				break;

			default:
				card.address = 0;
				card.transfer_count = 0;
				break;
		}
	}


	u32 read32_GCDATAIN(u8 PROCNUM)
	{
		nds_dscard& card = MMU.dscard[PROCNUM];

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

					//note that even if desmume was booting from firmware, and reading this chip ID to store in main memory,
					//this still works, since it will have read 00 originally and then read 00 to validate.
					return gameInfo.chipID;
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

					//it seems that etrian odyssey 3 doesnt work unless we mask this to cart size.
					//but, a thought: does the internal rom address counter register wrap around? we may be making a mistake by keeping the extra precision
					//but there is no test case yet
					u32 address = card.address & (gameInfo.mask);

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

			// Nand Init?
			case 0x94:
				return 0; //Unsure what to return here so return 0 for now

			// Nand Status?
			case 0xD6:
				//0x80 == busy
				// Made in Ore/WarioWare D.I.Y. need set value to 0x80
				return 0x80; //0x20 == ready

			default:
				return 0;
		} //switch(card.command[0])
	} //read32_GCDATAIN

};

ISlot1Interface* construct_Slot1_Retail_NAND() { return new Slot1_Retail_NAND(); }


