/*  Copyright (C) 2006 thoduv
    Copyright (C) 2006-2007 Theo Berkau
	Copyright (C) 2008-2009 DeSmuME team

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "types.h"
#include "mc.h"
#include "movie.h"
#include "readwrite.h"

#define FW_CMD_READ             0x3
#define FW_CMD_WRITEDISABLE     0x4
#define FW_CMD_READSTATUS       0x5
#define FW_CMD_WRITEENABLE      0x6
#define FW_CMD_PAGEWRITE        0xA

#define BM_CMD_AUTODETECT       0xFF
#define BM_CMD_WRITESTATUS      0x1
#define BM_CMD_WRITELOW         0x2
#define BM_CMD_READLOW          0x3
#define BM_CMD_WRITEDISABLE     0x4
#define BM_CMD_READSTATUS       0x5
#define BM_CMD_WRITEENABLE      0x6
#define BM_CMD_WRITEHIGH        0xA
#define BM_CMD_READHIGH         0xB

/* FLASH*/
#define COMM_PAGE_WRITE		0x0A
#define COMM_PAGE_ERASE		0xDB
#define COMM_SECTOR_ERASE	0xD8
#define COMM_CHIP_ERASE		0xC7
#define CARDFLASH_READ_BYTES_FAST	0x0B    /* Not used*/
#define CARDFLASH_DEEP_POWDOWN		0xB9    /* Not used*/
#define CARDFLASH_WAKEUP			0xAB    /* Not used*/

//this should probably be 0xFF but we're using 0x00 until we find out otherwise
//(no$ appears definitely to initialized to 0xFF)
static const u8 kUninitializedSaveDataValue = 0x00; 

static const char* kDesmumeSaveCookie = "|-DESMUME SAVE-|";

static const u32 saveSizes[] = {512,8*1024,64*1024,256*1024,512*1024,32*1024};
static const u32 saveSizes_count = ARRAY_SIZE(saveSizes);


void mc_init(memory_chip_t *mc, int type)
{
        mc->com = 0;
        mc->addr = 0;
        mc->addr_shift = 0;
        mc->data = NULL;
        mc->size = 0;
        mc->write_enable = FALSE;
        mc->writeable_buffer = FALSE;
        mc->type = type;
        mc->autodetectsize = 0;
                               
        switch(mc->type)
        {
           case MC_TYPE_EEPROM1:
              mc->addr_size = 1;
              break;
           case MC_TYPE_EEPROM2:
           case MC_TYPE_FRAM:
              mc->addr_size = 2;
              break;
           case MC_TYPE_FLASH:
              mc->addr_size = 3;
              break;
           default: break;
        }
}

u8 *mc_alloc(memory_chip_t *mc, u32 size)
{
	u8 *buffer;
	buffer = new u8[size];
	memset(buffer,0,size);

	mc->data = buffer;
	if(!buffer) { return NULL; }
	mc->size = size;
	mc->writeable_buffer = TRUE;

	return buffer;
}

void mc_free(memory_chip_t *mc)
{
    if(mc->data) delete[] mc->data;
    mc_init(mc, 0);
}

void mc_reset_com(memory_chip_t *mc)
{
   size_t elems_written = 0;

   if (mc->type == MC_TYPE_AUTODETECT && mc->com == BM_CMD_AUTODETECT)
   {
      u32 addr, size;

	  LOG("autodetectsize = %d\n",mc->autodetectsize);

      if (mc->autodetectsize == (32768+2))
      {
         // FRAM
         addr = (mc->autodetectbuf[0] << 8) | mc->autodetectbuf[1];
         mc->type = MC_TYPE_FRAM;
         mc->size = MC_SIZE_256KBITS;
      }
	  else if (mc->autodetectsize == (512+3))
      {
         // Flash 4Mbit
         addr = (mc->autodetectbuf[0] << 16) |
                    (mc->autodetectbuf[1] << 8) |
                     mc->autodetectbuf[2];
         mc->type = MC_TYPE_FLASH;
         mc->size = MC_SIZE_4MBITS;
      }
      else if (mc->autodetectsize == (256+3))
      {
         // Flash 2Mbit
         addr = (mc->autodetectbuf[0] << 16) |
                    (mc->autodetectbuf[1] << 8) |
                     mc->autodetectbuf[2];
         mc->type = MC_TYPE_FLASH;
         mc->size = MC_SIZE_2MBITS;
      }
      else if ((mc->autodetectsize == (128+2)) || (mc->autodetectsize == (64+2)) || (mc->autodetectsize == (40+2)))
      {
         // 512 Kbit EEPROM
         addr = (mc->autodetectbuf[0] << 8) | mc->autodetectbuf[1];
         mc->type = MC_TYPE_EEPROM2;
         mc->size = MC_SIZE_512KBITS;
      }
      else if ((mc->autodetectsize == (32+2)) || (mc->autodetectsize == (16+2)))
      {
         // 64 Kbit EEPROM
         addr = (mc->autodetectbuf[0] << 8) | mc->autodetectbuf[1];
         mc->type = MC_TYPE_EEPROM2;
         mc->size = MC_SIZE_64KBITS;
      }
      else if (mc->autodetectsize == (16+1))
      {
         // 4 Kbit EEPROM
         addr = mc->autodetectbuf[0];
         mc->type = MC_TYPE_EEPROM1;
         mc->size = MC_SIZE_4KBITS;
      }
      else
      {
		  /*
         // Assume it's a Flash non-page write 
         LOG("Flash detected(guessed). autodetectsize = %d\n", mc->autodetectsize);
         addr = (mc->autodetectbuf[0] << 16) |
                    (mc->autodetectbuf[1] << 8) |
                     mc->autodetectbuf[2];
         mc->type = MC_TYPE_FLASH;
         mc->size = MC_SIZE_2MBITS;
		 */
	 // 64 Kbit EEPROM
         addr = (mc->autodetectbuf[0] << 8) | mc->autodetectbuf[1];
         mc->type = MC_TYPE_EEPROM2;
         mc->size = MC_SIZE_64KBITS;
      }

      size = mc->autodetectsize;
      mc_realloc(mc, mc->type, mc->size);
      memcpy(mc->data+addr, mc->autodetectbuf+mc->addr_size, size-mc->addr_size);
      mc->autodetectsize = 0;
      mc->write_enable = FALSE;

      // Generate file
      if ((mc->fp = fopen(mc->filename, "wb+")) != NULL)
         elems_written += fwrite((void *)mc->data, 1, mc->size, mc->fp);
   }
   else if ((mc->com == BM_CMD_WRITELOW) || (mc->com == FW_CMD_PAGEWRITE))
   {      
      if(!mc->fp)
         mc->fp = fopen(mc->filename, "wb+");

      if (mc->fp)
      {
         fseek(mc->fp, 0, SEEK_SET);
         elems_written += fwrite((void *)mc->data, 1, mc->size, mc->fp); // FIXME
      }
      // FIXME: desmume silently ignores not having opened save-file
      mc->write_enable = FALSE;
   }

   mc->com = 0;
}

void mc_realloc(memory_chip_t *mc, int type, u32 size)
{
    if(mc->data) delete[] mc->data;
    mc_init(mc, type);
    mc_alloc(mc, size);     
}

void mc_load_file(memory_chip_t *mc, const char* filename)
{
   long size;
   int type = -1;
   FILE* file;
   size_t elems_read;

   if(movieMode != MOVIEMODE_INACTIVE) {
	    mc->filename = strdup(filename);
		return;
   }
   else
	   file = fopen(filename, "rb+");

   if(file == NULL)
   {
      mc->filename = strdup(filename);
      return;
   }

   fseek(file, 0, SEEK_END);
   size = ftell(file);
   fseek(file, 0, SEEK_SET);

   if (mc->type == MC_TYPE_AUTODETECT)
   {
      if (size == MC_SIZE_4KBITS)
         type = MC_TYPE_EEPROM1;
      else if (size == MC_SIZE_64KBITS)
         type = MC_TYPE_EEPROM2;
      else if (size == MC_SIZE_256KBITS)
         type = MC_TYPE_FRAM;
      else if (size == MC_SIZE_512KBITS)
         type = MC_TYPE_EEPROM2;
      else if (size >= MC_SIZE_2MBITS)
         type = MC_TYPE_FLASH;
	  else if (size >= MC_SIZE_4MBITS)
         type = MC_TYPE_FLASH;

      if (type != -1)
         mc_realloc(mc, type, size);
   }

   if ((u32)size > mc->size)
      size = mc->size;
   elems_read = fread (mc->data, 1, size, file);
   mc->fp = file;
}

int mc_load_duc(memory_chip_t *mc, const char* filename)
{
   long size;
   int type = -1;
   char id[16];
   FILE* file = fopen(filename, "rb");
   size_t elems_read = 0;
   if(file == NULL)
      return 0;

   fseek(file, 0, SEEK_END);
   size = ftell(file) - 500;
   fseek(file, 0, SEEK_SET);

   // Make sure we really have the right file
   elems_read += fread((void *)id, sizeof(char), 16, file);

   if (memcmp(id, "ARDS000000000001", 16) != 0)
   {
      fclose(file);
      return 0;
   }

   // Alright, it's time to load the file
   if (mc->type == MC_TYPE_AUTODETECT)
   {
      if (size == MC_SIZE_4KBITS)
         type = MC_TYPE_EEPROM1;
      else if (size == MC_SIZE_64KBITS)
         type = MC_TYPE_EEPROM2;
      else if (size == MC_SIZE_256KBITS)
         type = MC_TYPE_FRAM;
      else if (size == MC_SIZE_512KBITS)
         type = MC_TYPE_EEPROM2;
      else if (size >= MC_SIZE_2MBITS)
         type = MC_TYPE_FLASH;
	  else if (size >= MC_SIZE_4MBITS)
         type = MC_TYPE_FLASH;

      if (type != -1)
         mc_realloc(mc, type, size);
   }

   if ((u32)size > mc->size)
      size = mc->size;
   // Skip the rest of the header since we don't need it
   fseek(file, 500, SEEK_SET);
   elems_read += fread (mc->data, 1, size, file);
   fclose(file);

   return 1;
}

u8 fw_transfer(memory_chip_t *mc, u8 data)
{
	if(mc->com == FW_CMD_READ || mc->com == FW_CMD_PAGEWRITE) /* check if we are in a command that needs 3 bytes address */
	{
		if(mc->addr_shift > 0)   /* if we got a complete address */
		{
			mc->addr_shift--;
			mc->addr |= data << (mc->addr_shift * 8); /* argument is a byte of address */
		}
		else    /* if we have received 3 bytes of address, proceed command */
		{
			switch(mc->com)
			{
				case FW_CMD_READ:
					if(mc->addr < mc->size)  /* check if we can read */
					{
						data = mc->data[mc->addr];       /* return byte */
						mc->addr++;      /* then increment address */
					}
					break;
					
				case FW_CMD_PAGEWRITE:
					if(mc->addr < mc->size)
					{
						mc->data[mc->addr] = data;       /* write byte */
						mc->addr++;
					}
					break;
			}
			
		}
	}
	else if(mc->com == FW_CMD_READSTATUS)
	{
		return (mc->write_enable ? 0x02 : 0x00);
	}
	else	/* finally, check if it's a new command */
	{
		switch(data)
		{
			case 0: break;	/* nothing */
			
			case FW_CMD_READ:    /* read command */
				mc->addr = 0;
				mc->addr_shift = 3;
				mc->com = FW_CMD_READ;
				break;
				
			case FW_CMD_WRITEENABLE:     /* enable writing */
				if(mc->writeable_buffer) { mc->write_enable = TRUE; }
				break;
				
			case FW_CMD_WRITEDISABLE:    /* disable writing */
				mc->write_enable = FALSE;
				break;
				
			case FW_CMD_PAGEWRITE:       /* write command */
				if(mc->write_enable)
				{
					mc->addr = 0;
					mc->addr_shift = 3;
					mc->com = FW_CMD_PAGEWRITE;
				}
				else { data = 0; }
				break;
			
			case FW_CMD_READSTATUS:  /* status register command */
				mc->com = FW_CMD_READSTATUS;
				break;
				
			default:
				LOG("Unhandled FW command: %02X\n", data);
				break;
		}
	}
	
	return data;
}	

u8 bm_transfer(memory_chip_t *mc, u8 data)
{
	if(mc->com == BM_CMD_READLOW || mc->com == BM_CMD_WRITELOW) /* check if we are in a command that needs multiple byte address */
	{
		if(mc->addr_shift > 0)   /* if we got a complete address */
		{
			mc->addr_shift--;
			mc->addr |= data << (mc->addr_shift * 8); /* argument is a byte of address */
		}
		else    /* if we have received all bytes of address, proceed command */
		{
			switch(mc->com)
			{
				case BM_CMD_READLOW:
					if(mc->addr < mc->size)  /* check if we can read */
					{
						//LOG("Read Backup Memory addr %08X(%02X)\n", mc->addr, mc->data[mc->addr]);
						data = mc->data[mc->addr];       /* return byte */
						mc->addr++;      /* then increment address */
					}
					break;
					
				case BM_CMD_WRITELOW:
					if(mc->addr < mc->size)
					{
						//LOG("Write Backup Memory addr %08X with %02X\n", mc->addr, data);
						mc->data[mc->addr] = data;       /* write byte */
						mc->addr++;
					}
					break;
			}
			
		}
	}
	else if(mc->com == BM_CMD_AUTODETECT)
	{
		// Store everything in a temporary
		mc->autodetectbuf[mc->autodetectsize] = data;
		mc->autodetectsize++;
		return 0;
	}
	else if(mc->com == BM_CMD_READSTATUS)
	{
		//LOG("Backup Memory Read Status: %02X\n", mc->write_enable << 1);
		return (mc->write_enable << 1);
	}
	else	/* finally, check if it's a new command */
	{
		switch(data)
		{
			case 0: break;	/* nothing */
			
			case BM_CMD_WRITELOW:       /* write command */
				if(mc->write_enable)
				{
					if(mc->type == MC_TYPE_AUTODETECT)
					{
						mc->com = BM_CMD_AUTODETECT;
						break;
					}

					mc->addr = 0;
					mc->addr_shift = mc->addr_size;
					mc->com = BM_CMD_WRITELOW;
				}
				else { data = 0; }
				break;

			case BM_CMD_READLOW:    /* read command */
				mc->addr = 0;           
				mc->addr_shift = mc->addr_size;
				mc->com = BM_CMD_READLOW;
				break;
								
			case BM_CMD_WRITEDISABLE:    /* disable writing */
				mc->write_enable = FALSE;
				break;
							
			case BM_CMD_READSTATUS:  /* status register command */
				mc->com = BM_CMD_READSTATUS;
				break;

			case BM_CMD_WRITEENABLE:     /* enable writing */
				if(mc->writeable_buffer) { mc->write_enable = TRUE; }
				break;

			case BM_CMD_WRITEHIGH:       /* write command that's only available on ST M95040-W that I know of */
				if(mc->write_enable)
				{
					if(mc->type == MC_TYPE_AUTODETECT)
					{
						mc->com = BM_CMD_AUTODETECT;
						break;
					}

					if (mc->type == MC_TYPE_EEPROM1)
						mc->addr = 0x100;
					else
						mc->addr = 0;
					mc->addr_shift = mc->addr_size;
					mc->com = BM_CMD_WRITELOW;
				}
				else { data = 0; }
				break;

			case BM_CMD_READHIGH:    /* read command that's only available on ST M95040-W that I know of */
				if (mc->type == MC_TYPE_EEPROM1)
					mc->addr = 0x100;
				else
					mc->addr = 0;
				mc->addr_shift = mc->addr_size;
				mc->com = BM_CMD_READLOW;

				break;

			default:
				LOG("TRANSFER: Unhandled Backup Memory command: %02X\n", data);
				break;
		}
	}
	
	return data;
}	


bool BackupDevice::save_state(std::ostream* os)
{
	int version = 0;
	write32le(version,os);
//	write32le(0,os); //reserved for type if i need it later
//	write32le(0,os); //reserved for size if i need it later
	write32le(write_enable,os);
	write32le(com,os);
	write32le(addr_size,os);
	write32le(addr_counter,os);
	write32le((u32)state,os);
	writebuffer(data,os);
	writebuffer(data_autodetect,os);
	return true;
}

bool BackupDevice::load_state(std::istream* is)
{
	int version;
	if(read32le(&version,is)!=1) return false;
	if(version==0) {
#if 0
		u32 size, type;
		read32le(&size,is);
		read32le(&type,is);
#endif
		read32le(&write_enable,is);
		read32le(&com,is);
		read32le(&addr_size,is);
		read32le(&addr_counter,is);
		u32 temp;
		read32le(&temp,is);
		state = (STATE)temp;
		readbuffer(data,is);
		readbuffer(data_autodetect,is);
	}
	return true;
}

//due to unfortunate shortcomings in the emulator architecture, 
//at reset-time, we won't have a filename to the .sav file.
//so the only difference between load_rom (init) and reset is that
//one of them saves the filename
void BackupDevice::load_rom(const char* filename)
{
	this->filename = filename;
	reset();
}

void BackupDevice::reset()
{
	state = DETECTING;
	data.resize(0);
	data_autodetect.resize(0);
	loadfile();
	flushPending = false;
}

void BackupDevice::close_rom() {}

void BackupDevice::reset_command()
{
	//for a performance hack, save files are only flushed after each reset command
	//(hopefully, after each page)
	if(flushPending)
	{
		flush();
		flushPending = false;
	}

	if(state == DETECTING && data_autodetect.size()>0)
	{
		//we can now safely detect the save address size
		u32 autodetect_size = data_autodetect.size();
		addr_size = autodetect_size - 1;
		if(autodetect_size==6) addr_size = 2; //castlevania dawn of sorrow
		if(autodetect_size==7) addr_size = 3; //advance wars dual strike 2mbit flash
		if(autodetect_size==31) addr_size = 3; //daigasso! band brothers  2mbit flash
		if(autodetect_size==258) addr_size = 2; //warioware touched
		if(autodetect_size==257) addr_size = 1; //yoshi touch & go
		if(autodetect_size==9) addr_size = 1; //star wars III
		if(autodetect_size==113) addr_size = 1; //space invaders revolution
		if(autodetect_size==33) addr_size = 1; //bomberman
		if(autodetect_size==65) addr_size = 1; //robots
		if(autodetect_size==66) addr_size = 2; //pokemon dash
		if(autodetect_size==22) addr_size = 2; //puyo pop fever
		if(autodetect_size==18) addr_size = 2; //lunar dragon song
		if(autodetect_size==17) addr_size = 1; //shrek super slam
		if(autodetect_size==109) addr_size = 1; //scooby-doo! unmasked
		if(addr_size>4)
		{
			INFO("RESET: Unexpected backup memory address size: %d\n",addr_size);
		}
		state = RUNNING;
		data_autodetect.resize(0);
	}

	com = 0;
}
u8 BackupDevice::data_command(u8 val)
{
	if(com == BM_CMD_READLOW || com == BM_CMD_WRITELOW)
	{
		//handle data or address
		if(state == DETECTING)
		{
			if(com == BM_CMD_WRITELOW)
			{
				LOG("Unexpected backup device initialization sequence using writes!\n");
			}

			//just buffer the data until we're no longer detecting
			data_autodetect.push_back(val);
			val = 0;
		}
		else
		{
			if(addr_counter<addr_size)
			{
				//continue building address
				addr <<= 8;
				addr |= val;
				addr_counter++;
			}
			else
			{
				//address is complete
				ensure(addr);
				if(com == BM_CMD_READLOW)
				{
					val = data[addr];
					//printf("read: %08X\n",addr);
				}
				else 
				{
					data[addr] = val;
					flushPending = true;
					//printf("writ: %08X\n",addr);
				}
				addr++;

			}
		}
	}
	else if(com == BM_CMD_READSTATUS)
	{
		//handle request to read status
		//LOG("Backup Memory Read Status: %02X\n", mc->write_enable << 1);
		return (write_enable << 1);
	}
	else
	{
		//there is no current command. receive one
		switch(val)
		{
			case 0: break; //??
			
			case BM_CMD_WRITEDISABLE:
				write_enable = FALSE;
				break;
							
			case BM_CMD_READSTATUS:
				com = BM_CMD_READSTATUS;
				break;

			case BM_CMD_WRITEENABLE:
				write_enable = TRUE;
				break;

			case BM_CMD_WRITELOW:
			case BM_CMD_READLOW:
				com = val;
				addr_counter = 0;
				addr = 0;
				break;

			case BM_CMD_WRITEHIGH:
			case BM_CMD_READHIGH:
				if(val == BM_CMD_WRITEHIGH) val = BM_CMD_WRITELOW;
				if(val == BM_CMD_READHIGH) val = BM_CMD_READLOW;
				com = val;
				addr_counter = 0;
				addr = 0;
				if(addr_size==1) {
					//"write command that's only available on ST M95040-W that I know of"
					//this makes sense, since this device would only have a 256 bytes address space with writelow
					//and writehigh would allow access to the upper 256 bytes
					//but it was detected in pokemon diamond also during the main save process
					addr = 0x1;
				}
				break;

			default:
				LOG("COMMAND: Unhandled Backup Memory command: %02X\n", val);
				break;
		}
	}
	return val;
}

//guarantees that the data buffer has room enough for a byte at the specified address
void BackupDevice::ensure(u32 addr)
{
	u32 size = data.size();
	if(size<addr+1)
	{
		data.resize(addr+1);
	}
	for(u32 i=size;i<=addr;i++)
		data[i] = kUninitializedSaveDataValue;
}


void BackupDevice::loadfile()
{
	FILE* inf = fopen(filename.c_str(),"rb");
	if(inf)
	{
		//scan for desmume save footer
		const u32 cookieLen = strlen(kDesmumeSaveCookie);
		char *sigbuf = new char[cookieLen];
		fseek(inf, -cookieLen, SEEK_END);
		fread(sigbuf,1,cookieLen,inf);
		int cmp = memcmp(sigbuf,kDesmumeSaveCookie,cookieLen);
		delete[] sigbuf;
		if(cmp)
		{
			//raw save file
			fseek(inf, 0, SEEK_END);
			int size = ftell(inf);
			fseek(inf, 0, SEEK_SET);
			data.resize(size);
			fread(&data[0],1,size,inf);
			fclose(inf);
			return;
		}
		//desmume format
		fseek(inf, -cookieLen, SEEK_END);
		fseek(inf, -4, SEEK_CUR);
		u32 version = 0xFFFFFFFF;
		read32le(&version,inf);
		if(version!=0) {
			LOG("Unknown save file format\n");
			return;
		}
		fseek(inf, -24, SEEK_CUR);
		struct {
			u32 size,padSize,type,addr_size,mem_size;
		} info;
		read32le(&info.size,inf);
		read32le(&info.padSize,inf);
		read32le(&info.type,inf);
		read32le(&info.addr_size,inf);
		read32le(&info.mem_size,inf);

		//establish the save data
		data.resize(info.size);
		fseek(inf, 0, SEEK_SET);
		fread(&data[0],1,info.size,inf); //read all the raw data we have
		state = RUNNING;
		addr_size = info.addr_size;
		//none of the other fields are used right now

		fclose(inf);
	}
	else
	{
		LOG("Missing save file %s\n",filename.c_str());
	}
}

u32 BackupDevice::addr_size_for_old_save_size(int bupmem_size)
{
	switch(bupmem_size) {
		case MC_SIZE_4KBITS: return 2; //1? hi command?
		case MC_SIZE_64KBITS: 
		case MC_SIZE_256KBITS:
		case MC_SIZE_512KBITS:
			return 2;
		case MC_SIZE_1MBITS:
		case MC_SIZE_2MBITS:
		case MC_SIZE_4MBITS:
		case MC_SIZE_8MBITS:
		case MC_SIZE_16MBITS:
		case MC_SIZE_64MBITS:
			return 3;
		default:
			return 0xFFFFFFFF;
	}
}

u32 BackupDevice::addr_size_for_old_save_type(int bupmem_type)
{
	switch(bupmem_type)
	{
		case MC_TYPE_EEPROM1:
			return 1;
		case MC_TYPE_EEPROM2:
		case MC_TYPE_FRAM:
              return 2;
		case MC_TYPE_FLASH:
			return 3;
		default:
			return 0xFFFFFFFF;
	}
}


void BackupDevice::load_old_state(u32 addr_size, u8* data, u32 datasize)
{
	state = RUNNING;
	this->addr_size = addr_size;
	this->data.resize(datasize);
	memcpy(&this->data[0],data,datasize);
}

void BackupDevice::flush()
{
	FILE* outf = fopen(filename.c_str(),"wb");
	if(outf)
	{
		fwrite(&data[0],1,data.size(),outf);
		
		//write the footer. we use a footer so that we can maximize the chance of the
		//save file being recognized as a raw save file by other emulators etc.
		
		//first, pad up to the next largest known save size.
		u32 size = data.size();
		int ctr=0;
		while(ctr<saveSizes_count && size > saveSizes[ctr]) ctr++;
		u32 padSize = saveSizes[ctr];

		for(u32 i=size;i<padSize;i++)
			fputc(kUninitializedSaveDataValue,outf);

		//this is just for humans to read
		fprintf(outf,"|<--Snip above here to create a raw sav by excluding this DeSmuME savedata footer:");

		//and now the actual footer
		write32le(size,outf); //the size of data that has actually been written
		write32le(padSize,outf); //the size we padded it to
		write32le(0,outf); //save memory type
		write32le(addr_size,outf);
		write32le(0,outf); //save memory size
		write32le(0,outf); //version number
		fprintf(outf, "%s", kDesmumeSaveCookie); //this is what we'll use to recognize the desmume format save


		fclose(outf);
	}
	else
	{
		LOG("Unable to open savefile %s\n",filename.c_str());
	}
}

bool BackupDevice::load_duc(const char* filename)
{
  long size;
   char id[16];
   FILE* file = fopen(filename, "rb");
   size_t elems_read = 0;
   if(file == NULL)
      return false;

   fseek(file, 0, SEEK_END);
   size = ftell(file) - 500;
   fseek(file, 0, SEEK_SET);

   // Make sure we really have the right file
   elems_read += fread((void *)id, sizeof(char), 16, file);

   if (memcmp(id, "ARDS000000000001", 16) != 0)
   {
	   LOG("Not recognized as a valid DUC file\n");
      fclose(file);
      return false;
   }
   // Skip the rest of the header since we don't need it
   fseek(file, 500, SEEK_SET);

   ensure((u32)size);

   fread(&data[0],1,size,file);
   fclose(file);

   flush();

   return true;

}
