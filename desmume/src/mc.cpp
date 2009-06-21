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
#include "NDSSystem.h"

//temporary hack until we have better error reporting facilities
#ifdef _MSC_VER
#include <windows.h>
#endif

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

static const u32 saveSizes[] = {512,8*1024,32*1024,64*1024,256*1024,512*1024,0xFFFFFFFF};
static const u32 saveSizes_count = ARRAY_SIZE(saveSizes);

//the lookup table from user save types to save parameters
static const int save_types[7][2] = {
        {MC_TYPE_AUTODETECT,1},
        {MC_TYPE_EEPROM1,MC_SIZE_4KBITS},
        {MC_TYPE_EEPROM2,MC_SIZE_64KBITS},
        {MC_TYPE_EEPROM2,MC_SIZE_512KBITS},
        {MC_TYPE_FRAM,MC_SIZE_256KBITS},
        {MC_TYPE_FLASH,MC_SIZE_2MBITS},
		{MC_TYPE_FLASH,MC_SIZE_4MBITS}
};

void backup_setManualBackupType(int type)
{
	CommonSettings.manualBackupType = type;
}

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

void fw_reset_com(memory_chip_t *mc)
{
	if(mc->com == FW_CMD_PAGEWRITE)
	{
		if (mc->fp)
		{
			fseek(mc->fp, 0, SEEK_SET);
			fwrite(mc->data, mc->size, 1, mc->fp);
		}

		mc->write_enable = FALSE;
	}

	mc->com = 0;
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
				printf("Unhandled FW command: %02X\n", data);
				break;
		}
	}
	
	return data;
}	

bool BackupDevice::save_state(std::ostream* os)
{
	int version = 0;
	write32le(version,os);
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

BackupDevice::BackupDevice()
{
}

//due to unfortunate shortcomings in the emulator architecture, 
//at reset-time, we won't have a filename to the .dsv file.
//so the only difference between load_rom (init) and reset is that
//one of them saves the filename
void BackupDevice::load_rom(const char* filename)
{
	isMovieMode = false;
	this->filename = filename;
	reset();
}

void BackupDevice::movie_mode()
{
	isMovieMode = true;
	reset();
}

void BackupDevice::reset()
{
	com = 0;
	addr = addr_counter = 0;
	flushPending = false;
	lazyFlushPending = false;
	data.resize(0);
	write_enable = FALSE;
	data_autodetect.resize(0);

	state = DETECTING;
	addr_size = 0;
	loadfile();

	//if the user has requested a manual choice for backup type, and we havent imported a raw save file, then apply it now
	if(state == DETECTING && CommonSettings.manualBackupType != MC_TYPE_AUTODETECT)
	{
		state = RUNNING;
		int savetype = save_types[CommonSettings.manualBackupType][0];
		int savesize = save_types[CommonSettings.manualBackupType][1];
		ensure((u32)savesize); //expand properly if necessary
		data.resize(savesize); //truncate if necessary
		addr_size = addr_size_for_old_save_type(savetype);
		flush();
	}
}

void BackupDevice::close_rom()
{
	flush();
}

void BackupDevice::reset_command()
{
	//for a performance hack, save files are only flushed after each reset command
	//(hopefully, after each page)
	if(flushPending)
	{
		flush();
		flushPending = false;
		lazyFlushPending = false;
	}

	if(state == DETECTING && data_autodetect.size()>0)
	{
		//we can now safely detect the save address size
		u32 autodetect_size = data_autodetect.size();

		printf("Autodetecting with autodetect_size=%d\n",autodetect_size);

		const u8 sm64_sig[] = {0x01,0x80,0x00,0x00};
		if(autodetect_size == 4 && !memcmp(&data_autodetect[0],sm64_sig,4))
		{
			addr_size = 2;
		}
		else //detect based on rules
			switch(autodetect_size)
			{
			case 0:
			case 1:
				printf("Catastrophic error while autodetecting save type.\nIt will need to be specified manually\n");
				#ifdef _MSC_VER
				MessageBox(0,"Catastrophic Error Code: Camel;\nyour save type has not been autodetected correctly;\nplease report to developers",0,0);
				#endif
				addr_size = 1; //choose 1 just to keep the busted savefile from growing too big
				break;
			case 2:
				 //the modern typical case for small eeproms
				addr_size = 1;
				break;
			case 3:
				//another modern typical case..
				//but unfortunately we select this case for spider-man 3, when what it meant to do was
				//present the archaic 1+2 case
				addr_size = 2;
				break;
			case 4:
				//a modern typical case
				addr_size = 3;
				break;
			default:
				//the archaic case: write the address and then some modulo-4 number of bytes
				//why modulo 4? who knows.
				addr_size = autodetect_size & 3;
				break;
			}

		state = RUNNING;
		data_autodetect.resize(0);
		flush();
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
				printf("Unexpected backup device initialization sequence using writes!\n");
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
				//if(addr_counter==addr_size) printf("ADR: %08X\n",addr);
			}
			else
			{
				//why does tomb raider underworld access 0x180 and go clear through to 0x280?
				//should this wrap around at 0 or at 0x100?
				if(addr_size == 1) addr &= 0x1FF; 

				//address is complete
				ensure(addr+1);
				if(com == BM_CMD_READLOW)
				{
					val = data[addr];
					//flushPending = true; //is this a good idea? it may slow stuff down, but it is helpful for debugging
					lazyFlushPending = true; //lets do this instead
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
				//printf("XLO: %08X\n",addr);
				com = val;
				addr_counter = 0;
				addr = 0;
				break;

			case BM_CMD_WRITEHIGH:
			case BM_CMD_READHIGH:
				//printf("XHI: %08X\n",addr);
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
				printf("COMMAND: Unhandled Backup Memory command: %02X\n", val);
				break;
		}
	}
	return val;
}

//guarantees that the data buffer has room enough for the specified number of bytes
void BackupDevice::ensure(u32 addr)
{
	u32 size = data.size();
	if(size<addr)
	{
		data.resize(addr);
	}
	for(u32 i=size;i<addr;i++)
		data[i] = kUninitializedSaveDataValue;
}


u32 BackupDevice::addr_size_for_old_save_size(int bupmem_size)
{
	switch(bupmem_size) {
		case MC_SIZE_4KBITS: 
			return 1;
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

	//dump back out as a dsv, just to keep things sane
	flush();
}


void BackupDevice::loadfile()
{
	//never use save files if we are in movie mode
	if(isMovieMode) return;
	if(filename.length() ==0) return; //No sense crashing if no filename supplied

	FILE* inf = fopen(filename.c_str(),"rb");
	if(!inf)
	{
		//no dsv found; we need to try auto-importing a file with .sav extension
		printf("DeSmuME .dsv save file not found. Trying to load an old raw .sav file.\n");
		
		//change extension to sav
		char tmp[MAX_PATH];
		strcpy(tmp,filename.c_str());
		tmp[strlen(tmp)-3] = 0;
		strcat(tmp,"sav");

		inf = fopen(tmp,"rb");
		if(!inf)
		{
			printf("Missing save file %s\n",filename.c_str());
			return;
		}
		fclose(inf);

		load_raw(tmp);
	}
	else
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
			//maybe it is a misnamed raw save file. try loading it that way
			printf("Not a DeSmuME .dsv save file. Trying to load as raw.\n");
			fclose(inf);
			load_raw(filename.c_str());
			return;
		}
		//desmume format
		fseek(inf, -cookieLen, SEEK_END);
		fseek(inf, -4, SEEK_CUR);
		u32 version = 0xFFFFFFFF;
		read32le(&version,inf);
		if(version!=0) {
			printf("Unknown save file format\n");
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
		if(info.size>0)
			fread(&data[0],1,info.size,inf); //read all the raw data we have
		state = RUNNING;
		addr_size = info.addr_size;
		//none of the other fields are used right now

		fclose(inf);
	}
}

bool BackupDevice::save_raw(const char* filename)
{
	FILE* outf = fopen(filename,"wb");
	if(!outf) return false;
	u32 size = data.size();
	u32 padSize = pad_up_size(size);
	if(data.size()>0)
		fwrite(&data[0],1,size,outf);
	for(u32 i=size;i<padSize;i++)
		fputc(kUninitializedSaveDataValue,outf);
	fclose(outf);
	return true;
}

u32 BackupDevice::pad_up_size(u32 startSize)
{
	u32 size = startSize;
	u32 ctr=0;
	while(ctr<saveSizes_count && size > saveSizes[ctr]) ctr++;
	u32 padSize = saveSizes[ctr];
	if(padSize == 0xFFFFFFFF)
	{
		printf("PANIC! Couldn't pad up save size. Refusing to pad.\n");
		padSize = startSize;
	}
	return padSize;
}

void BackupDevice::lazy_flush()
{
	if(flushPending || lazyFlushPending)
	{
		lazyFlushPending = flushPending = false;
		flush();
	}
}

void BackupDevice::flush()
{
	//never use save files if we are in movie mode
	if(isMovieMode) return;

	FILE* outf = fopen(filename.c_str(),"wb");
	if(outf)
	{
		if(data.size()>0)
			fwrite(&data[0],1,data.size(),outf);
		
		//write the footer. we use a footer so that we can maximize the chance of the
		//save file being recognized as a raw save file by other emulators etc.
		
		//first, pad up to the next largest known save size.
		u32 size = data.size();
		u32 padSize = pad_up_size(size);

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
		printf("Unable to open savefile %s\n",filename.c_str());
	}
}

void BackupDevice::raw_applyUserSettings(u32& size)
{
	//respect the user's choice of backup memory type
	if(CommonSettings.manualBackupType == MC_TYPE_AUTODETECT)
		addr_size = addr_size_for_old_save_size(size);
	else
	{
		int savetype = save_types[CommonSettings.manualBackupType][0];
		int savesize = save_types[CommonSettings.manualBackupType][1];
		addr_size = addr_size_for_old_save_type(savetype);
		if((u32)savesize<size) size = savesize;
	}

	state = RUNNING;
}


bool BackupDevice::load_raw(const char* filename)
{
	FILE* inf = fopen(filename,"rb");
	fseek(inf, 0, SEEK_END);
	u32 size = (u32)ftell(inf);
	fseek(inf, 0, SEEK_SET);
	
	raw_applyUserSettings(size);

	data.resize(size);
	fread(&data[0],1,size,inf);
	fclose(inf);

	//dump back out as a dsv, just to keep things sane
	flush();

	return true;
}


bool BackupDevice::load_duc(const char* filename)
{
  u32 size;
   char id[16];
   FILE* file = fopen(filename, "rb");
   if(file == NULL)
      return false;

   fseek(file, 0, SEEK_END);
   size = (u32)ftell(file) - 500;
   fseek(file, 0, SEEK_SET);

   // Make sure we really have the right file
   fread((void *)id, sizeof(char), 16, file);

   if (memcmp(id, "ARDS000000000001", 16) != 0)
   {
	   printf("Not recognized as a valid DUC file\n");
      fclose(file);
      return false;
   }
   // Skip the rest of the header since we don't need it
   fseek(file, 500, SEEK_SET);

   raw_applyUserSettings(size);

   ensure((u32)size);

   fread(&data[0],1,size,file);
   fclose(file);

   //choose 

   flush();

   return true;

}

bool BackupDevice::load_movie(std::istream* is) {

	const u32 cookieLen = strlen(kDesmumeSaveCookie);

	is->seekg(-cookieLen, std::ios::end);
	is->seekg(-4, std::ios::cur);

	u32 version = 0xFFFFFFFF;
	is->read((char*)&version,4);
	if(version!=0) {
		printf("Unknown save file format\n");
		return false;
	}
	is->seekg(-24, std::ios::cur);

	struct{
		u32 size,padSize,type,addr_size,mem_size;
	}info;

	is->read((char*)&info.size,4);
	is->read((char*)&info.padSize,4);
	is->read((char*)&info.type,4);
	is->read((char*)&info.addr_size,4);
	is->read((char*)&info.mem_size,4);

	//establish the save data
	data.resize(info.size);
	is->seekg(0, std::ios::beg);
	if(info.size>0)
		is->read((char*)&data[0],info.size);

	state = RUNNING;
	addr_size = info.addr_size;
	//none of the other fields are used right now

	return true;
}
