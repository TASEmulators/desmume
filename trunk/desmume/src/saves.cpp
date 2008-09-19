/*  Copyright (C) 2006 Normmatt
    Copyright (C) 2006 Theo Berkau
    Copyright (C) 2007 Pascal Giard

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

#ifdef HAVE_LIBZ
#include <zlib.h>
#endif
#include <stdio.h>
#include <string.h>
#include "saves.h"
#include "MMU.h"
#include "NDSSystem.h"
#include <sys/stat.h>
#include <time.h>
#include <fstream>

#include "memorystream.h"
#include "readwrite.h"
#include "gfx3d.h"


//void*v is actually a void** which will be indirected before reading
//since this isnt supported right now, it is declared in here to make things compile
#define SS_INDIRECT            0x80000000

savestates_t savestates[NB_STATES];

#define SAVESTATE_VERSION       10
static const char* magic = "DeSmuME SState\0";

#ifndef MAX_PATH
#define MAX_PATH 256
#endif


SFORMAT SF_ARM7[]={
	{ &NDS_ARM7.instruction, 4|SS_RLSB, "7INS" },
	{ &NDS_ARM7.instruct_adr, 4|SS_RLSB, "7INA" },
	{ &NDS_ARM7.next_instruction, 4|SS_RLSB, "7INN" },
	{ NDS_ARM7.R, 4|SS_MULT(16), "7REG" },
	{ &NDS_ARM7.CPSR, 4|SS_RLSB, "7CPS" },
	{ &NDS_ARM7.SPSR, 4|SS_RLSB, "7SPS" },
	{ &NDS_ARM7.R13_usr, 4|SS_RLSB, "7DUS" },
	{ &NDS_ARM7.R14_usr, 4|SS_RLSB, "7EUS" },
	{ &NDS_ARM7.R13_svc, 4|SS_RLSB, "7DSV" },
	{ &NDS_ARM7.R14_svc, 4|SS_RLSB, "7ESV" },
	{ &NDS_ARM7.R13_abt, 4|SS_RLSB, "7DAB" },
	{ &NDS_ARM7.R14_abt, 4|SS_RLSB, "7EAB" },
	{ &NDS_ARM7.R13_und, 4|SS_RLSB, "7DUN" },
	{ &NDS_ARM7.R14_und, 4|SS_RLSB, "7EUN" },
	{ &NDS_ARM7.R13_irq, 4|SS_RLSB, "7DIR" },
	{ &NDS_ARM7.R14_irq, 4|SS_RLSB, "7EIR" },
	{ &NDS_ARM7.R8_fiq, 4|SS_RLSB, "78FI" },
	{ &NDS_ARM7.R9_fiq, 4|SS_RLSB, "79FI" },
	{ &NDS_ARM7.R10_fiq, 4|SS_RLSB, "7AFI" },
	{ &NDS_ARM7.R11_fiq, 4|SS_RLSB, "7BFI" },
	{ &NDS_ARM7.R12_fiq, 4|SS_RLSB, "7CFI" },
	{ &NDS_ARM7.R13_fiq, 4|SS_RLSB, "7DFI" },
	{ &NDS_ARM7.R14_fiq, 4|SS_RLSB, "7EFI" },
	{ &NDS_ARM7.SPSR_svc, 4|SS_RLSB, "7SVC" },
	{ &NDS_ARM7.SPSR_abt, 4|SS_RLSB, "7ABT" },
	{ &NDS_ARM7.SPSR_und, 4|SS_RLSB, "7UND" },
	{ &NDS_ARM7.SPSR_irq, 4|SS_RLSB, "7IRQ" },
	{ &NDS_ARM7.SPSR_fiq, 4|SS_RLSB, "7FIQ" },
	{ &NDS_ARM7.intVector, 4|SS_RLSB, "7int" },
	{ &NDS_ARM7.LDTBit, 1, "7LDT" },
	{ &NDS_ARM7.waitIRQ, 4|SS_RLSB, "7Wai" },
	{ &NDS_ARM7.wIRQ, 4|SS_RLSB, "7wIR" },
	{ &NDS_ARM7.wirq, 4|SS_RLSB, "7wir" },
	{ 0 }
};

SFORMAT SF_ARM9[]={
	{ &NDS_ARM9.instruction, 4|SS_RLSB, "9INS" },
	{ &NDS_ARM9.instruct_adr, 4|SS_RLSB, "9INA" },
	{ &NDS_ARM9.next_instruction, 4|SS_RLSB, "9INN" },
	{ NDS_ARM9.R, 4|SS_MULT(16), "9REG" },
	{ &NDS_ARM9.CPSR, 4|SS_RLSB, "9CPS" },
	{ &NDS_ARM9.SPSR, 4|SS_RLSB, "9SPS" },
	{ &NDS_ARM9.R13_usr, 4|SS_RLSB, "9DUS" },
	{ &NDS_ARM9.R14_usr, 4|SS_RLSB, "9EUS" },
	{ &NDS_ARM9.R13_svc, 4|SS_RLSB, "9DSV" },
	{ &NDS_ARM9.R14_svc, 4|SS_RLSB, "9ESV" },
	{ &NDS_ARM9.R13_abt, 4|SS_RLSB, "9DAB" },
	{ &NDS_ARM9.R14_abt, 4|SS_RLSB, "9EAB" },
	{ &NDS_ARM9.R13_und, 4|SS_RLSB, "9DUN" },
	{ &NDS_ARM9.R14_und, 4|SS_RLSB, "9EUN" },
	{ &NDS_ARM9.R13_irq, 4|SS_RLSB, "9DIR" },
	{ &NDS_ARM9.R14_irq, 4|SS_RLSB, "9EIR" },
	{ &NDS_ARM9.R8_fiq, 4|SS_RLSB, "98FI" },
	{ &NDS_ARM9.R9_fiq, 4|SS_RLSB, "99FI" },
	{ &NDS_ARM9.R10_fiq, 4|SS_RLSB, "9AFI" },
	{ &NDS_ARM9.R11_fiq, 4|SS_RLSB, "9BFI" },
	{ &NDS_ARM9.R12_fiq, 4|SS_RLSB, "9CFI" },
	{ &NDS_ARM9.R13_fiq, 4|SS_RLSB, "9DFI" },
	{ &NDS_ARM9.R14_fiq, 4|SS_RLSB, "9EFI" },
	{ &NDS_ARM9.SPSR_svc, 4|SS_RLSB, "9SVC" },
	{ &NDS_ARM9.SPSR_abt, 4|SS_RLSB, "9ABT" },
	{ &NDS_ARM9.SPSR_und, 4|SS_RLSB, "9UND" },
	{ &NDS_ARM9.SPSR_irq, 4|SS_RLSB, "9IRQ" },
	{ &NDS_ARM9.SPSR_fiq, 4|SS_RLSB, "9FIQ" },
	{ &NDS_ARM9.intVector, 4|SS_RLSB, "9int" },
	{ &NDS_ARM9.LDTBit, 1, "9LDT" },
	{ &NDS_ARM9.waitIRQ, 4|SS_RLSB, "9Wai" },
	{ &NDS_ARM9.wIRQ, 4|SS_RLSB, "9wIR" },
	{ &NDS_ARM9.wirq, 4|SS_RLSB, "9wir" },
	{ 0 }
};

SFORMAT SF_MEM[]={
	{ ARM9Mem.ARM9_ITCM, 0x8000, "ITCM" },
	{ ARM9Mem.ARM9_DTCM, 0x4000, "DTCM" },
	{ ARM9Mem.MAIN_MEM, 0x400000, "WRAM" },
	{ ARM9Mem.ARM9_REG, 0x10000, "9REG" },
	{ ARM9Mem.ARM9_VMEM, 0x800, "VMEM" },
	{ ARM9Mem.ARM9_OAM, 0x800, "OAMS" },
	{ ARM9Mem.ARM9_ABG, 0x80000, "ABGM" },
	{ ARM9Mem.ARM9_BBG, 0x20000, "BBGM" },
	{ ARM9Mem.ARM9_AOBJ, 0x40000, "AOBJ" },
	{ ARM9Mem.ARM9_BOBJ, 0x20000, "BOBJ" },
	{ ARM9Mem.ARM9_LCD, 0xA4000, "LCDM" },
	{ MMU.ARM7_ERAM, 0x10000, "ERAM" },
	{ MMU.ARM7_REG, 0x10000, "7REG" },
	{ MMU.ARM7_WIRAM, 0x10000, "WIRA" },
	{ MMU.SWIRAM, 0x8000, "SWIR" },
	{ MMU.CART_RAM, SRAM_SIZE, "SRAM" },
	{ 0 }
};

SFORMAT SF_NDS[]={
	{ &nds.ARM9Cycle, 4|SS_RLSB, "_9CY" },
	{ &nds.ARM7Cycle, 4|SS_RLSB, "_7CY" },
	{ &nds.cycles, 4|SS_RLSB, "_CYC" },
	{ nds.timerCycle, 4|SS_MULT(8), "_TCY" },
	{ nds.timerOver, 4|SS_MULT(8), "_TOV" },
	{ &nds.nextHBlank, 4|SS_RLSB, "_NHB" },
	{ &nds.VCount, 4|SS_RLSB, "_VCT" },
	{ &nds.old, 4|SS_RLSB, "_OLD" },
	{ &nds.diff, 4|SS_RLSB, "_DIF" },
	{ &nds.lignerendu, 4|SS_RLSB, "_LIG" },
	{ &nds.touchX, 2|SS_RLSB, "_TPX" },
	{ &nds.touchY, 2|SS_RLSB, "_TPY" },
	{ 0 }
};

/* Format time and convert to string */
char * format_time(time_t cal_time)
{
  struct tm *time_struct;
  static char string[30];

  time_struct=localtime(&cal_time);
  strftime(string, sizeof string, "%Y-%m-%d %H:%M", time_struct);

  return(string);
}

void clear_savestates()
{
  u8 i;
  for( i = 0; i < NB_STATES; i++ )
    savestates[i].exists = FALSE;
}

/* Scan for existing savestates and update struct */
void scan_savestates()
{
  struct stat sbuf;
  char filename[MAX_PATH];
  u8 i;

  clear_savestates();

  for( i = 1; i <= NB_STATES; i++ )
    {
      strncpy(filename, szRomBaseName,MAX_PATH);
	  if (strlen(filename) + strlen(".dst") + strlen("-2147483648") /* = biggest string for i */ >MAX_PATH) return ;
      sprintf(filename+strlen(filename), "%d.dst", i);
      if( stat(filename,&sbuf) == -1 ) continue;
      savestates[i-1].exists = TRUE;
      strncpy(savestates[i-1].date, format_time(sbuf.st_mtime),40-strlen(savestates[i-1].date));
    }

  return ;
}

void savestate_slot(int num)
{
   struct stat sbuf;
   char filename[MAX_PATH];

   strncpy(filename, szRomBaseName,MAX_PATH);
   if (strlen(filename) + strlen(".dst") + strlen("-2147483648") /* = biggest string for num */ >MAX_PATH) return ;
   sprintf(filename+strlen(filename), "%d.dst", num);
   savestate_save(filename);

   savestates[num-1].exists = TRUE;
   if( stat(filename,&sbuf) == -1 ) return;
   strncpy(savestates[num-1].date, format_time(sbuf.st_mtime),40-strlen(savestates[num-1].date));
}

void loadstate_slot(int num)
{
   char filename[MAX_PATH];
   strncpy(filename, szRomBaseName,MAX_PATH);
   if (strlen(filename) + strlen(".dst") + strlen("-2147483648") /* = biggest string for num */ >MAX_PATH) return ;
   sprintf(filename+strlen(filename), "%d.dst", num);
   savestate_load(filename);
}

u8 sram_read (u32 address) {
	address = address - SRAM_ADDRESS;
	
	if ( address > SRAM_SIZE )
		return 0;

	return MMU.CART_RAM[address];

}

void sram_write (u32 address, u8 value) {

	address = address - SRAM_ADDRESS;

	if ( address < SRAM_SIZE )
		MMU.CART_RAM[address] = value;
	
}

int sram_load (const char *file_name) {
	
	FILE *file;

	file = fopen ( file_name, "rb" );
	if( file == NULL )
		return 0;

	fread ( MMU.CART_RAM, SRAM_SIZE, 1, file );

	fclose ( file );

	return 1;

}

int sram_save (const char *file_name) {

	FILE *file;

	file = fopen ( file_name, "wb" );
	if( file == NULL )
		return 0;

	fwrite ( MMU.CART_RAM, SRAM_SIZE, 1, file );

	fclose ( file );

	return 1;

}

static SFORMAT *CheckS(SFORMAT *sf, u32 tsize, char *desc)
{
	while(sf->v)
	{
		if(sf->s==~0)		// Link to another SFORMAT structure.
		{
			SFORMAT *tmp;
			if((tmp= CheckS((SFORMAT *)sf->v, tsize, desc) ))
				return(tmp);
			sf++;
			continue;
		}
		if(!memcmp(desc,sf->desc,4))
		{
			if(tsize!=(sf->s))
				return(0);
			return(sf);
		}
		sf++;
	}
	return(0);
}


static bool ReadStateChunk(std::istream* is, SFORMAT *sf, int size)
{
	SFORMAT *tmp;
	int temp = is->tellg();

	while(is->tellg()<temp+size)
	{
		u32 tsize;
		char toa[4];
		is->read(toa,4);
		if(is->fail())
			return false;

		read32le(&tsize,is);

		if((tmp=CheckS(sf,tsize,toa)))
		{
			int count = SS_UNMULT(tsize);
			int size = tsize & ~SS_FLAGS;
			bool rlsb = (count!=0);

			if(count == 0) count=1;

			for(int i=0;i<count;i++) {

				if(tmp->s&SS_INDIRECT)
					is->read(*(char **)tmp->v,size);
				else
					is->read((char *)tmp->v + i*size,size);

				#ifndef LOCAL_LE
					if(rlsb)
						FlipByteOrder((u8*)tmp->v + i*size,size);
				#endif
			}
		}
		else
			is->seekg(tsize,std::ios::cur);
	} // while(...)
	return true;
}



static int SubWrite(std::ostream* os, SFORMAT *sf)
{
	uint32 acc=0;

	while(sf->v)
	{
		if(sf->s==~0)		//Link to another struct
		{
			uint32 tmp;

			if(!(tmp=SubWrite(os,(SFORMAT *)sf->v)))
				return(0);
			acc+=tmp;
			sf++;
			continue;
		}

		int count = SS_UNMULT(sf->s);
		int size = sf->s & ~SS_FLAGS;
		bool rlsb = (count!=0);

		acc+=8;			//Description + size

		if(count==0) count=1;

		acc += count * size;

		if(os)			//Are we writing or calculating the size of this block?
		{
			os->write(sf->desc,4);
			write32le(sf->s,os);

			for(int i=0;i<count;i++) {
			
				#ifndef LOCAL_LE
				if(rlsb)
					FlipByteOrder(sf->v,sf->s&(~SS_FLAGS));
				#endif

				if(sf->s&SS_INDIRECT)
					os->write(*(char **)sf->v,size);
				else
					os->write((char*)sf->v + i*size,size);

				//Now restore the original byte order.
				#ifndef LOCAL_LE
				if(rlsb)
					FlipByteOrder(sf->v,sf->s&(~SS_FLAGS));
				#endif

			}
		}
		sf++;
	}

	return(acc);
}

static int savestate_WriteChunk(std::ostream* os, int type, SFORMAT *sf)
{
	write32le(type,os);
	if(!sf) return 4;
	int bsize = SubWrite((std::ostream*)0,sf);
	write32le(bsize,os);
	FILE* outf;

	if(!SubWrite(os,sf))
	{
		return 8;
	}
	return (bsize+8);
}

static void savestate_WriteChunk(std::ostream* os, int type, void (*saveproc)(std::ostream* os))
{
	//get the size
	memorystream mstemp;
	saveproc(&mstemp);
	mstemp.flush();
	u32 size = mstemp.size();

	//write the type, size, and data
	write32le(type,os);
	write32le(size,os);
	os->write(mstemp.buf(),size);
}

static void writechunks(std::ostream* os);

bool savestate_save(std::ostream* outstream, int compressionLevel)
{
	//generate the savestate in memory first
	memorystream ms;
	std::ostream* os = (std::ostream*)&ms;
	writechunks(os);
	ms.flush();

	//save the length of the file
	u32 len = ms.size();

	u32 comprlen = -1;
	u8* cbuf = (u8*)ms.buf();

#ifdef HAVE_LIBZ
	//compress the data
	int error = Z_OK;
	if(compressionLevel != Z_NO_COMPRESSION)
	{
		//worst case compression.
		//zlib says "0.1% larger than sourceLen plus 12 bytes"
		comprlen = (len>>9)+12 + len;
		cbuf = new u8[comprlen];
		error = compress2(cbuf,&comprlen,(u8*)ms.buf(),len,compressionLevel);
	}
#endif

	//dump the header
	outstream->write(magic,16);
	write32le(SAVESTATE_VERSION,outstream);
	write32le(DESMUME_VERSION_NUMERIC,outstream); //desmume version
	write32le(len,outstream); //uncompressed length
	write32le(comprlen,outstream); //compressed length (-1 if it is not compressed)

	outstream->write((char*)cbuf,comprlen==-1?len:comprlen);
	if(cbuf != (uint8*)ms.buf()) delete[] cbuf;
	return error == Z_OK;
}

bool savestate_save (const char *file_name)
{
	memorystream ms;
	if(!savestate_save(&ms, Z_DEFAULT_COMPRESSION))
		return false;
	ms.flush();
	FILE* file = fopen(file_name,"wb");
	if(file)
	{
		fwrite(ms.buf(),1,ms.size(),file);
		fclose(file);
		return true;
	} else return false;
}

//u8 GPU_screen[4*256*192];

static void writechunks(std::ostream* os) {
	savestate_WriteChunk(os,1,SF_ARM9);
	savestate_WriteChunk(os,2,SF_ARM7);
	savestate_WriteChunk(os,3,SF_MEM);
	savestate_WriteChunk(os,4,SF_NDS);
	savestate_WriteChunk(os,5,gpu_savestate);
	savestate_WriteChunk(os,60,SF_GFX3D);
	savestate_WriteChunk(os,61,gfx3d_savestate);
	savestate_WriteChunk(os,0xFFFFFFFF,(SFORMAT*)0);
}

static bool ReadStateChunks(std::istream* is, s32 totalsize)
{
	bool ret = true;
	while(totalsize > 0)
	{
		uint32 size;
		u32 t;
		if(!read32le(&t,is))  { ret=false; break; }
		if(t == 0xFFFFFFFF) goto done;
		if(!read32le(&size,is))  { ret=false; break; }
		switch(t)
		{
			case 1: if(!ReadStateChunk(is,SF_ARM9,size)) ret=false; break;
			case 2: if(!ReadStateChunk(is,SF_ARM7,size)) ret=false; break;
			case 3: if(!ReadStateChunk(is,SF_MEM,size)) ret=false; break;
			case 4: if(!ReadStateChunk(is,SF_NDS,size)) ret=false; break;
			case 5: if(!gpu_loadstate(is)) ret=false; break;
			case 60: if(!ReadStateChunk(is,SF_GFX3D,size)) ret=false; break;
			case 61: if(!gfx3d_loadstate(is)) ret=false; break;
			default:
				ret=false;
				break;
		}
		if(!ret) return false;
	}
done:

	return ret;
}

static void loadstate()
{
    // This should regenerate the vram banks
    for (int i = 0; i < 0xA; i++)
       MMU_write8(ARMCPU_ARM9, 0x04000240+i, MMU_read8(ARMCPU_ARM9, 0x04000240+i));

    // This should regenerate the graphics power control register
    MMU_write16(ARMCPU_ARM9, 0x04000304, MMU_read16(ARMCPU_ARM9, 0x04000304));

	// This should regenerate the graphics configuration
    for (int i = REG_BASE_DISPA; i<=REG_BASE_DISPA + 0x7F; i+=2)
	MMU_write16(ARMCPU_ARM9, i, MMU_read16(ARMCPU_ARM9, i));
    for (int i = REG_BASE_DISPB; i<=REG_BASE_DISPB + 0x7F; i+=2)
	MMU_write16(ARMCPU_ARM9, i, MMU_read16(ARMCPU_ARM9, i));
}

bool savestate_load(std::istream* is)
{
	char header[16];
	is->read(header,16);
	if(is->fail() || memcmp(header,magic,16))
		return false;

	u32 ssversion,dversion,len,comprlen;
	if(!read32le(&ssversion,is)) return false;
	if(!read32le(&dversion,is)) return false;
	if(!read32le(&len,is)) return false;
	if(!read32le(&comprlen,is)) return false;

	if(ssversion != SAVESTATE_VERSION) return false;

	std::vector<char> buf(len);

	if(comprlen != 0xFFFFFFFF) {
		std::vector<char> cbuf(comprlen);
		is->read(&cbuf[0],comprlen);
		if(is->fail()) return false;

		uLongf uncomprlen = len;
		int error = uncompress((uint8*)&buf[0],&uncomprlen,(uint8*)&cbuf[0],comprlen);
		if(error != Z_OK || uncomprlen != len)
			return false;
	} else {
		is->read((char*)&buf[0],len);
	}

	memorystream mstemp(&buf);
	bool x = ReadStateChunks(&mstemp,(s32)len);

	loadstate();

	return x;
}

bool savestate_load(const char *file_name)
{
	std::ifstream f;
	f.open(file_name,std::ios_base::binary|std::ios_base::in);
	if(!f) return false;

	return savestate_load(&f);
}