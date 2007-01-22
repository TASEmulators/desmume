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

#define SAVESTATE_VERSION       010

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

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
      strcpy(filename, szRomBaseName);
      sprintf(filename+strlen(filename), "%d.dst", i);
      if( stat(filename,&sbuf) == -1 ) continue;
      savestates[i-1].exists = TRUE;
      strcpy(savestates[i-1].date, format_time(sbuf.st_mtime));
    }

  return 1;
}

void savestate_slot(int num)
{
   struct stat sbuf;
   char filename[MAX_PATH];

   strcpy(filename, szRomBaseName);
   sprintf(filename+strlen(filename), "%d.dst", num);
   savestate_save(filename);

   savestates[num-1].exists = TRUE;
   if( stat(filename,&sbuf) == -1 ) return;
   strcpy(savestates[num-1].date, format_time(sbuf.st_mtime));
}

void loadstate_slot(int num)
{
   char filename[MAX_PATH];
   strcpy(filename, szRomBaseName);
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

int savestate_load (const char *file_name)    {
#ifdef HAVE_LIBZ
	
	gzFile file;
        char idstring[30];
        u8 version;
        int i;

	file = gzopen( file_name, "rb" );
	if( file == NULL )
		return 0;

        memset(idstring, 0, 30);
        gzgets(file, idstring, 23);

        if (strncmp("DeSmuME Savestate File", idstring, 22) != 0)
        {
           gzclose (file);
           return 0;
        }

        version = gzgetc(file);

        // Read ARM7 cpu registers
        gzread(file, &NDS_ARM7.proc_ID, sizeof(u32));
        gzread(file, &NDS_ARM7.instruction, sizeof(u32));
        gzread(file, &NDS_ARM7.instruct_adr, sizeof(u32));
        gzread(file, &NDS_ARM7.next_instruction, sizeof(u32));
        gzread(file, NDS_ARM7.R, sizeof(u32) * 16);
        gzread(file, &NDS_ARM7.CPSR, sizeof(Status_Reg));
        gzread(file, &NDS_ARM7.SPSR, sizeof(Status_Reg));
        gzread(file, &NDS_ARM7.R13_usr, sizeof(u32));
        gzread(file, &NDS_ARM7.R14_usr, sizeof(u32));
        gzread(file, &NDS_ARM7.R13_svc, sizeof(u32));
        gzread(file, &NDS_ARM7.R14_svc, sizeof(u32));
        gzread(file, &NDS_ARM7.R13_abt, sizeof(u32));
        gzread(file, &NDS_ARM7.R14_abt, sizeof(u32));
        gzread(file, &NDS_ARM7.R13_und, sizeof(u32));
        gzread(file, &NDS_ARM7.R14_und, sizeof(u32));
        gzread(file, &NDS_ARM7.R13_irq, sizeof(u32));
        gzread(file, &NDS_ARM7.R14_irq, sizeof(u32));
        gzread(file, &NDS_ARM7.R8_fiq, sizeof(u32));
        gzread(file, &NDS_ARM7.R9_fiq, sizeof(u32));
        gzread(file, &NDS_ARM7.R10_fiq, sizeof(u32));
        gzread(file, &NDS_ARM7.R11_fiq, sizeof(u32));
        gzread(file, &NDS_ARM7.R12_fiq, sizeof(u32));
        gzread(file, &NDS_ARM7.R13_fiq, sizeof(u32));
        gzread(file, &NDS_ARM7.R14_fiq, sizeof(u32));
        gzread(file, &NDS_ARM7.SPSR_svc, sizeof(Status_Reg));
        gzread(file, &NDS_ARM7.SPSR_abt, sizeof(Status_Reg));
        gzread(file, &NDS_ARM7.SPSR_und, sizeof(Status_Reg));
        gzread(file, &NDS_ARM7.SPSR_irq, sizeof(Status_Reg));
        gzread(file, &NDS_ARM7.SPSR_fiq, sizeof(Status_Reg));
        gzread(file, &NDS_ARM7.intVector, sizeof(u32));
        gzread(file, &NDS_ARM7.LDTBit, sizeof(u8));
        gzread(file, &NDS_ARM7.waitIRQ, sizeof(BOOL));
        gzread(file, &NDS_ARM7.wIRQ, sizeof(BOOL));
        gzread(file, &NDS_ARM7.wirq, sizeof(BOOL));

        // Read ARM9 cpu registers
        gzread(file, &NDS_ARM9.proc_ID, sizeof(u32));
        gzread(file, &NDS_ARM9.instruction, sizeof(u32));
        gzread(file, &NDS_ARM9.instruct_adr, sizeof(u32));
        gzread(file, &NDS_ARM9.next_instruction, sizeof(u32));
        gzread(file, NDS_ARM9.R, sizeof(u32) * 16);
        gzread(file, &NDS_ARM9.CPSR, sizeof(Status_Reg));
        gzread(file, &NDS_ARM9.SPSR, sizeof(Status_Reg));
        gzread(file, &NDS_ARM9.R13_usr, sizeof(u32));
        gzread(file, &NDS_ARM9.R14_usr, sizeof(u32));
        gzread(file, &NDS_ARM9.R13_svc, sizeof(u32));
        gzread(file, &NDS_ARM9.R14_svc, sizeof(u32));
        gzread(file, &NDS_ARM9.R13_abt, sizeof(u32));
        gzread(file, &NDS_ARM9.R14_abt, sizeof(u32));
        gzread(file, &NDS_ARM9.R13_und, sizeof(u32));
        gzread(file, &NDS_ARM9.R14_und, sizeof(u32));
        gzread(file, &NDS_ARM9.R13_irq, sizeof(u32));
        gzread(file, &NDS_ARM9.R14_irq, sizeof(u32));
        gzread(file, &NDS_ARM9.R8_fiq, sizeof(u32));
        gzread(file, &NDS_ARM9.R9_fiq, sizeof(u32));
        gzread(file, &NDS_ARM9.R10_fiq, sizeof(u32));
        gzread(file, &NDS_ARM9.R11_fiq, sizeof(u32));
        gzread(file, &NDS_ARM9.R12_fiq, sizeof(u32));
        gzread(file, &NDS_ARM9.R13_fiq, sizeof(u32));
        gzread(file, &NDS_ARM9.R14_fiq, sizeof(u32));
        gzread(file, &NDS_ARM9.SPSR_svc, sizeof(Status_Reg));
        gzread(file, &NDS_ARM9.SPSR_abt, sizeof(Status_Reg));
        gzread(file, &NDS_ARM9.SPSR_und, sizeof(Status_Reg));
        gzread(file, &NDS_ARM9.SPSR_irq, sizeof(Status_Reg));
        gzread(file, &NDS_ARM9.SPSR_fiq, sizeof(Status_Reg));
        gzread(file, &NDS_ARM9.intVector, sizeof(u32));
        gzread(file, &NDS_ARM9.LDTBit, sizeof(u8));
        gzread(file, &NDS_ARM9.waitIRQ, sizeof(BOOL));
        gzread(file, &NDS_ARM9.wIRQ, sizeof(BOOL));
        gzread(file, &NDS_ARM9.wirq, sizeof(BOOL));

        // Read in other internal variables that are important
        gzread (file, &nds, sizeof(NDSSystem));

        // Read in memory/registers specific to the ARM9
        gzread (file, ARM9Mem.ARM9_ITCM, 0x8000);
        gzread (file ,ARM9Mem.ARM9_DTCM, 0x4000);
        gzread (file ,ARM9Mem.ARM9_WRAM, 0x1000000);
        gzread (file, ARM9Mem.MAIN_MEM, 0x400000);
        gzread (file, ARM9Mem.ARM9_REG, 0x10000);
        gzread (file, ARM9Mem.ARM9_VMEM, 0x800);
        gzread (file, ARM9Mem.ARM9_OAM, 0x800);    
        gzread (file, ARM9Mem.ARM9_ABG, 0x80000);
        gzread (file, ARM9Mem.ARM9_BBG, 0x20000);
        gzread (file, ARM9Mem.ARM9_AOBJ, 0x40000);
        gzread (file, ARM9Mem.ARM9_BOBJ, 0x20000);
        gzread (file, ARM9Mem.ARM9_LCD, 0xA4000);
    
        // Read in memory/registers specific to the ARM7
        gzread (file, MMU.ARM7_ERAM, 0x10000);
        gzread (file, MMU.ARM7_REG, 0x10000);
        gzread (file, MMU.ARM7_WIRAM, 0x10000);

        // Read in shared memory
        gzread (file, MMU.SWIRAM, 0x8000);

        // Internal variable states should be regenerated

        // This should regenerate the vram banks
        for (i = 0; i < 0xA; i++)
           MMU_write8(ARMCPU_ARM9, 0x04000240+i, MMU_read8(ARMCPU_ARM9, 0x04000240+i));

        // This should regenerate the graphics power control register
        MMU_write16(ARMCPU_ARM9, 0x04000304, MMU_read16(ARMCPU_ARM9, 0x04000304));

        GPU_setVideoProp(MainScreen.gpu, MMU_read32(ARMCPU_ARM9, 0x04000000));
        GPU_setVideoProp(SubScreen.gpu, MMU_read32(ARMCPU_ARM9, 0x04000000));
        gzclose (file);

	return 1;
#else
        return 0;
#endif
}

int savestate_save (const char *file_name)    {
#ifdef HAVE_LIBZ
	gzFile file;

	file = gzopen( file_name, "wb" );
	if( file == NULL )
		return 0;

        gzputs(file, "DeSmuME Savestate File");
        gzputc(file, SAVESTATE_VERSION);

        // Save ARM7 cpu registers
        gzwrite(file, &NDS_ARM7.proc_ID, sizeof(u32));
        gzwrite(file, &NDS_ARM7.instruction, sizeof(u32));
        gzwrite(file, &NDS_ARM7.instruct_adr, sizeof(u32));
        gzwrite(file, &NDS_ARM7.next_instruction, sizeof(u32));
        gzwrite(file, NDS_ARM7.R, sizeof(u32) * 16);
        gzwrite(file, &NDS_ARM7.CPSR, sizeof(Status_Reg));
        gzwrite(file, &NDS_ARM7.SPSR, sizeof(Status_Reg));
        gzwrite(file, &NDS_ARM7.R13_usr, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R14_usr, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R13_svc, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R14_svc, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R13_abt, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R14_abt, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R13_und, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R14_und, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R13_irq, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R14_irq, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R8_fiq, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R9_fiq, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R10_fiq, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R11_fiq, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R12_fiq, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R13_fiq, sizeof(u32));
        gzwrite(file, &NDS_ARM7.R14_fiq, sizeof(u32));
        gzwrite(file, &NDS_ARM7.SPSR_svc, sizeof(Status_Reg));
        gzwrite(file, &NDS_ARM7.SPSR_abt, sizeof(Status_Reg));
        gzwrite(file, &NDS_ARM7.SPSR_und, sizeof(Status_Reg));
        gzwrite(file, &NDS_ARM7.SPSR_irq, sizeof(Status_Reg));
        gzwrite(file, &NDS_ARM7.SPSR_fiq, sizeof(Status_Reg));
        gzwrite(file, &NDS_ARM7.intVector, sizeof(u32));
        gzwrite(file, &NDS_ARM7.LDTBit, sizeof(u8));
        gzwrite(file, &NDS_ARM7.waitIRQ, sizeof(BOOL));
        gzwrite(file, &NDS_ARM7.wIRQ, sizeof(BOOL));
        gzwrite(file, &NDS_ARM7.wirq, sizeof(BOOL));

        // Save ARM9 cpu registers
        gzwrite(file, &NDS_ARM9.proc_ID, sizeof(u32));
        gzwrite(file, &NDS_ARM9.instruction, sizeof(u32));
        gzwrite(file, &NDS_ARM9.instruct_adr, sizeof(u32));
        gzwrite(file, &NDS_ARM9.next_instruction, sizeof(u32));
        gzwrite(file, NDS_ARM9.R, sizeof(u32) * 16);
        gzwrite(file, &NDS_ARM9.CPSR, sizeof(Status_Reg));
        gzwrite(file, &NDS_ARM9.SPSR, sizeof(Status_Reg));
        gzwrite(file, &NDS_ARM9.R13_usr, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R14_usr, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R13_svc, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R14_svc, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R13_abt, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R14_abt, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R13_und, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R14_und, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R13_irq, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R14_irq, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R8_fiq, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R9_fiq, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R10_fiq, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R11_fiq, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R12_fiq, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R13_fiq, sizeof(u32));
        gzwrite(file, &NDS_ARM9.R14_fiq, sizeof(u32));
        gzwrite(file, &NDS_ARM9.SPSR_svc, sizeof(Status_Reg));
        gzwrite(file, &NDS_ARM9.SPSR_abt, sizeof(Status_Reg));
        gzwrite(file, &NDS_ARM9.SPSR_und, sizeof(Status_Reg));
        gzwrite(file, &NDS_ARM9.SPSR_irq, sizeof(Status_Reg));
        gzwrite(file, &NDS_ARM9.SPSR_fiq, sizeof(Status_Reg));
        gzwrite(file, &NDS_ARM9.intVector, sizeof(u32));
        gzwrite(file, &NDS_ARM9.LDTBit, sizeof(u8));
        gzwrite(file, &NDS_ARM9.waitIRQ, sizeof(BOOL));
        gzwrite(file, &NDS_ARM9.wIRQ, sizeof(BOOL));
        gzwrite(file, &NDS_ARM9.wirq, sizeof(BOOL));

        // Save other internal variables that are important
        gzwrite (file, &nds, sizeof(NDSSystem));

        // Save memory/registers specific to the ARM9
        gzwrite (file, ARM9Mem.ARM9_ITCM, 0x8000);
        gzwrite (file ,ARM9Mem.ARM9_DTCM, 0x4000);
        gzwrite (file ,ARM9Mem.ARM9_WRAM, 0x1000000);
        gzwrite (file, ARM9Mem.MAIN_MEM, 0x400000);
        gzwrite (file, ARM9Mem.ARM9_REG, 0x10000);
        gzwrite (file, ARM9Mem.ARM9_VMEM, 0x800);
        gzwrite (file, ARM9Mem.ARM9_OAM, 0x800);    
        gzwrite (file, ARM9Mem.ARM9_ABG, 0x80000);
        gzwrite (file, ARM9Mem.ARM9_BBG, 0x20000);
        gzwrite (file, ARM9Mem.ARM9_AOBJ, 0x40000);
        gzwrite (file, ARM9Mem.ARM9_BOBJ, 0x20000);
        gzwrite (file, ARM9Mem.ARM9_LCD, 0xA4000);
    
        // Save memory/registers specific to the ARM7
        gzwrite (file, MMU.ARM7_ERAM, 0x10000);
        gzwrite (file, MMU.ARM7_REG, 0x10000);
        gzwrite (file, MMU.ARM7_WIRAM, 0x10000);

        // Save shared memory
        gzwrite (file, MMU.SWIRAM, 0x8000);
        gzclose (file);

	return 1;
#else
        return 0;
#endif
}

