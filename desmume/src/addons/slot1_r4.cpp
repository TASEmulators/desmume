///*
//	Copyright (C) 2010-2013 DeSmuME team
//
//	This file is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 2 of the License, or
//	(at your option) any later version.
//
//	This file is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
//*/
//
//#include <time.h>
//
//#include "../slot1.h"
//#include "../registers.h"
//#include "../MMU.h"
//#include "../NDSSystem.h"
//#include "../emufile.h"
//
//class Slot1_R4 : public ISlot1Interface
//{
//private:
//	EMUFILE *img;
//	u32 write_count;
//	u32 write_enabled;
//
//public:
//	Slot1_R4()
//		: img(NULL)
//		, write_count(0)
//		, write_enabled(0)
//	{
//	}
//
//	virtual Slot1Info const* info()
//	{
//		static Slot1InfoSimple info("R4","Slot1 R4 emulation");
//		return &info;
//	}
//
//
//	//called once when the emulator starts up, or when the device springs into existence
//	virtual bool init()
//	{
//		//strange to do this here but we need to make sure its done at some point
//		srand(time(NULL));
//		return true;
//	}
//	
//	virtual void connect()
//	{
//		img = slot1_GetFatImage();
//
//		if(!img)
//			INFO("slot1 fat not successfully mounted\n");
//	}
//
//	//called when the emulator disconnects the device
//	virtual void disconnect()
//	{
//		img = NULL;
//	}
//	
//	//called when the emulator shuts down, or when the device disappears from existence
//	virtual void shutdown()
//	{ 
//	}
//
//
//	virtual u32 read32(u8 PROCNUM, u32 adr)
//	{
//		switch(adr)
//		{
//		case REG_GCDATAIN:
//			return read32_GCDATAIN();
//		default:
//			return 0;
//		}
//	}
//
//
//	virtual void write32(u8 PROCNUM, u32 adr, u32 val)
//	{
//		switch(adr)
//		{
//			case REG_GCROMCTRL:
//				write32_GCROMCTRL(val);
//				break;
//			case REG_GCDATAIN:
//				write32_GCDATAIN(val);
//				break;
//		}
//	}
//
//private:
//
//	u32 read32_GCDATAIN()
//	{
//		nds_dscard& card = MMU.dscard[0];
//		
//		u32 val;
//
//		switch(card.command[0])
//		{
//			//Get ROM chip ID
//			case 0x90:
//			case 0xB8:
//				val = 0xFC2;
//				break;
//
//			case 0xB0:
//				val = 0x1F4;
//				break;
//			case 0xB9:
//				val = (rand() % 100) ? 0x1F4 : 0;
//				break;
//			case 0xBB:
//			case 0xBC:
//				val = 0;
//				break;
//			case 0xBA:
//				//INFO("Read from sd at sector %08X at adr %08X ",card.address/512,ftell(img));
//				img->fread(&val, 4);
//				//INFO("val %08X\n",val);
//				break;
//
//			default:
//				val = 0;
//		}
//
//		/*INFO("READ CARD command: %02X%02X%02X%02X% 02X%02X%02X%02X RET: %08X  ", 
//							card.command[0], card.command[1], card.command[2], card.command[3],
//							card.command[4], card.command[5], card.command[6], card.command[7],
//							val);
//		INFO("FROM: %08X  LR: %08X\n", NDS_ARM9.instruct_adr, NDS_ARM9.R[14]);*/
//
//
//		return val;
//	} //read32_GCDATAIN
//
//
//	void write32_GCROMCTRL(u32 val)
//	{
//		nds_dscard& card = MMU.dscard[0];
//
//		switch(card.command[0])
//		{
//			case 0xB0:
//				break;
//			case 0xB9:
//			case 0xBA:
//				card.address = 	(card.command[1] << 24) | (card.command[2] << 16) | (card.command[3] << 8) | card.command[4];
//				img->fseek(card.address,SEEK_SET);
//				break;
//			case 0xBB:
//				write_enabled = 1;
//				write_count = 0x80;
//			case 0xBC:
//				card.address = 	(card.command[1] << 24) | (card.command[2] << 16) | (card.command[3] << 8) | card.command[4];
//				img->fseek(card.address,SEEK_SET);
//				break;
//		}
//	}
//
//	void write32_GCDATAIN(u32 val)
//	{
//		nds_dscard& card = MMU.dscard[0];
//		//bool log=false;
//
//		memcpy(&card.command[0], &MMU.MMU_MEM[0][0x40][0x1A8], 8);
//
//		//last_write_count = write_count;
//		if(card.command[4])
//		{
//			// transfer is done
//			T1WriteLong(MMU.MMU_MEM[0][0x40], 0x1A4,val & 0x7F7FFFFF);
//
//			// if needed, throw irq for the end of transfer
//			if(MMU.AUX_SPI_CNT & 0x4000)
//				NDS_makeIrq(ARMCPU_ARM9, IRQ_BIT_GC_TRANSFER_COMPLETE);
//
//			return;
//		}
//
//		switch(card.command[0])
//		{
//			case 0xBB:
//			{
//				if(write_count && write_enabled)
//				{
//					img->fwrite(&val, 4);
//					img->fflush();
//					write_count--;
//				}
//				break;
//			}
//			default:
//				break;
//		}
//
//		if(write_count==0)
//		{
//			write_enabled = 0;
//
//			// transfer is done
//			T1WriteLong(MMU.MMU_MEM[0][0x40], 0x1A4,val & 0x7F7FFFFF);
//
//			// if needed, throw irq for the end of transfer
//			if(MMU.AUX_SPI_CNT & 0x4000)
//				NDS_makeIrq(ARMCPU_ARM9, IRQ_BIT_GC_TRANSFER_COMPLETE);
//		}
//
//		/*if(log)
//		{
//			INFO("WRITE CARD command: %02X%02X%02X%02X%02X%02X%02X%02X\t", 
//							card.command[0], card.command[1], card.command[2], card.command[3],
//							card.command[4], card.command[5], card.command[6], card.command[7]);
//			INFO("FROM: %08X\t", NDS_ARM9.instruct_adr);
//			INFO("VAL: %08X\n", val);
//		}*/
//	}
//
//};
//
//ISlot1Interface* construct_Slot1_R4() { return new Slot1_R4(); }