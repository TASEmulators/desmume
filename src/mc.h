/*  Copyright (C) 2006 thoduv
    Copyright (C) 2006 Theo Berkau

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

#ifndef __FW_H__
#define __FW_H__

#include <stdio.h>
#include <vector>
#include <string>
#include "types.h"

#define MC_TYPE_AUTODETECT      0x0
#define MC_TYPE_EEPROM1         0x1
#define MC_TYPE_EEPROM2         0x2
#define MC_TYPE_FLASH           0x3
#define MC_TYPE_FRAM            0x4

#define MC_SIZE_4KBITS                  0x000200
#define MC_SIZE_64KBITS                 0x002000
#define MC_SIZE_256KBITS                0x008000
#define MC_SIZE_512KBITS                0x010000
#define MC_SIZE_1MBITS                  0x020000
#define MC_SIZE_2MBITS                  0x040000
#define MC_SIZE_4MBITS                  0x080000
#define MC_SIZE_8MBITS                  0x100000
#define MC_SIZE_16MBITS                 0x200000
#define MC_SIZE_64MBITS                 0x800000

typedef struct
{
	u8 com;	/* persistent command actually handled */
        u32 addr;        /* current address for reading/writing */
        u8 addr_shift;   /* shift for address (since addresses are transfered by 3 bytes units) */
        u8 addr_size;    /* size of addr when writing/reading */
	
	BOOL write_enable;	/* is write enabled ? */
	
        u8 *data;       /* memory data */
        u32 size;       /* memory size */
	BOOL writeable_buffer;	/* is "data" writeable ? */
        int type; /* type of Memory */
        char *filename;
        FILE *fp;
        u8 autodetectbuf[32768];
        int autodetectsize;
} memory_chip_t;

//the new backup system by zeromus
class BackupDevice
{
public:
	BackupDevice();

	//signals the save system that we are in our regular mode, loading up a rom. initializes for that case.
	void load_rom(const char* filename);
	//signals the save system that we are in MOVIE mode. doesnt load up a rom, and never saves it. initializes for that case.
	void movie_mode();

	void reset();
	void close_rom();

	bool save_state(std::ostream* os);
	bool load_state(std::istream* is);
	
	//commands from mmu
	void reset_command();
	u8 data_command(u8);

	//this info was saved before the last reset (used for savestate compatibility)
	struct SavedInfo
	{
		u32 addr_size;
	} savedInfo;

	//and these are used by old savestates
	void load_old_state(u32 addr_size, u8* data, u32 datasize);
	static u32 addr_size_for_old_save_size(int bupmem_size);
	static u32 addr_size_for_old_save_type(int bupmem_type);

	static u32 pad_up_size(u32 startSize);
	void raw_applyUserSettings(u32& size);

	bool load_duc(const char* filename);
	bool load_raw(const char* filename);
	bool save_raw(const char* filename);
	bool load_movie(std::istream* is);

	//call me once a second or so to lazy flush the save data
	//here's the reason for this system: we want to dump save files when theyre READ
	//so that we have a better idea earlier on how large they are. but it slows things down
	//way too much if we flush whenever we read.
	void lazy_flush();

private:
	BOOL write_enable;	//is write enabled?
	u32 com;	//persistent command actually handled
	u32 addr_size, addr_counter;
	u32 addr;
	bool isMovieMode;

	std::string filename;
	std::vector<u8> data;
	std::vector<u8> data_autodetect;
	enum STATE {
		DETECTING = 0, RUNNING = 1
	} state;

	void loadfile();
	bool _loadfile(const char *fname);
	void ensure(u32 addr);
	void flush();

	bool flushPending, lazyFlushPending;
};

#define NDS_FW_SIZE_V1 (256 * 1024)		/* size of fw memory on nds v1 */
#define NDS_FW_SIZE_V2 (512 * 1024)		/* size of fw memory on nds v2 */

void mc_init(memory_chip_t *mc, int type);    /* reset and init values for memory struct */
u8 *mc_alloc(memory_chip_t *mc, u32 size);  /* alloc mc memory */
void mc_realloc(memory_chip_t *mc, int type, u32 size);      /* realloc mc memory */
void mc_load_file(memory_chip_t *mc, const char* filename); /* load save file and setup fp */
void mc_free(memory_chip_t *mc);    /* delete mc memory */
void fw_reset_com(memory_chip_t *mc);       /* reset communication with mc */
u8 fw_transfer(memory_chip_t *mc, u8 data);

void backup_setManualBackupType(int type);

#endif /*__FW_H__*/

