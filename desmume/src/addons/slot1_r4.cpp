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
#include "../MMU.h"
#include "../NDSSystem.h"
#include <time.h>

static FILE *img = NULL;
static u32 write_count = 0;
static u32 write_enabled = 0;
static int old_addr = 0;
static void init_r4_flash()
{
	srand(time(NULL));

	if (!img)
		img = fopen("DLDI_R4DS.img", "r+b");

	if(!img)
	{
		INFO("DLDI_R4DS.img not found\n");
	}
}

static void info(char *info) { strcpy(info, "Slot1 R4 Emulation"); }
static void config(void) {}

static BOOL init()
{ 
	init_r4_flash();
	return TRUE;
}

static void reset() {}

static void close() {}


static void write08(u32 adr, u8 val) {}
static void write16(u32 adr, u16 val) {}

static void write32_GCROMCTRL(u32 val)
{
	nds_dscard& card = MMU.dscard[0];

	switch(card.command[0])
	{
		case 0xB0:
			break;
		case 0xB9:
		case 0xBA:
			card.address = 	(card.command[1] << 24) | (card.command[2] << 16) | (card.command[3] << 8) | card.command[4];
			fseek(img,card.address,SEEK_SET);
			break;
		case 0xBB:
			write_enabled = 1;
			write_count = 0x80;
		case 0xBC:
			card.address = 	(card.command[1] << 24) | (card.command[2] << 16) | (card.command[3] << 8) | card.command[4];
			fseek(img,card.address,SEEK_SET);
			break;
	}
}

static void write32_GCDATAIN(u32 val)
{
	nds_dscard& card = MMU.dscard[0];
	//bool log=false;

	memcpy(&card.command[0], &MMU.MMU_MEM[0][0x40][0x1A8], 8);

	//last_write_count = write_count;
	if(card.command[4])
	{
		// transfer is done
		T1WriteLong(MMU.MMU_MEM[0][0x40], 0x1A4,val & 0x7F7FFFFF);

		// if needed, throw irq for the end of transfer
		if(MMU.AUX_SPI_CNT & 0x4000)
			NDS_makeInt(0, 19);

		return;
	}

	switch(card.command[0])
	{
		case 0xBB:
		{
			if(write_count && write_enabled)
			{
				fwrite(&val, 1, 4, img);
				fflush(img);
				write_count--;
			}
			break;
		}
		default:
			break;
	}

	if(write_count==0)
	{
		write_enabled = 0;

		// transfer is done
		T1WriteLong(MMU.MMU_MEM[0][0x40], 0x1A4,val & 0x7F7FFFFF);

		// if needed, throw irq for the end of transfer
		if(MMU.AUX_SPI_CNT & 0x4000)
			NDS_makeInt(0, 19);
	}

	/*if(log)
	{
		INFO("WRITE CARD command: %02X%02X%02X%02X%02X%02X%02X%02X\t", 
						card.command[0], card.command[1], card.command[2], card.command[3],
						card.command[4], card.command[5], card.command[6], card.command[7]);
		INFO("FROM: %08X\t", NDS_ARM9.instruct_adr);
		INFO("VAL: %08X\n", val);
	}*/
}

static void write32(u32 adr, u32 val)
{
	switch(adr)
	{
		case REG_GCROMCTRL:
			write32_GCROMCTRL(val);
			break;
		case REG_GCDATAIN:
			write32_GCDATAIN(val);
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
	
	u32 val;

	switch(card.command[0])
	{
		//Get ROM chip ID
		case 0x90:
		case 0xB8:
			val = 0xFC2;
			break;

		case 0xB0:
			val = 0x1F4;
			break;
		case 0xB9:
			val = (rand() % 100) ? 0x1F4 : 0;
			break;
		case 0xBB:
		case 0xBC:
			val = 0;
			break;
		case 0xBA:
			//INFO("Read from sd at sector %08X at adr %08X ",card.address/512,ftell(img));
			fread(&val, 1, 4, img);
			//INFO("val %08X\n",val);
			break;

		default:
			val = 0;
	}

	/*INFO("READ CARD command: %02X%02X%02X%02X% 02X%02X%02X%02X RET: %08X  ", 
						card.command[0], card.command[1], card.command[2], card.command[3],
						card.command[4], card.command[5], card.command[6], card.command[7],
						val);
	INFO("FROM: %08X  LR: %08X\n", NDS_ARM9.instruct_adr, NDS_ARM9.R[14]);*/


	return val;
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

ADDONINTERFACE slot1R4 = {
	"Slot1R4",
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
	info};
