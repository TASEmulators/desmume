/*
	Copyright (C) 2009-2015 DeSmuME Team

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

#ifndef _FIRMWARE_H_
#define _FIRMWARE_H_

#include <string>
#include "types.h"

#define NDS_FW_SIZE_V1 (256 * 1024)		/* size of fw memory on nds v1 */
#define NDS_FW_SIZE_V2 (512 * 1024)		/* size of fw memory on nds v2 */

// the count of bytes (user settings) copied from the firmware into memory (for easy access) during the firmware boot process
#define NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT 0x70

//extension of the firmware user settings file
#define FW_CONFIG_FILE_EXT "dfc"

struct NDS_fw_config_data;

class CFIRMWARE
{
private:
	u8		*tmp_data9;
	u8		*tmp_data7;
	u32		size9, size7;
	u32		userDataAddr;

	u16		getBootCodeCRC16();
	u32		decrypt(const u8 *in, u8* &out);
	u32		decompress(const u8 *in, u8* &out);

	bool	successLoad;

public:
	CFIRMWARE(): size9(0), size7(0), ARM9bootAddr(0), ARM7bootAddr(0), patched(0), userDataAddr(0x3FE00), successLoad(false) {};
	
	bool load();
	bool unpack();
	bool loadSettings();
	bool saveSettings();

	static std::string GetExternalFilePath();

	u32 getID() { return header.fw_identifier; }
	bool loaded() { return successLoad; }
	void *getTouchCalibrate();

	struct HEADER
	{
		u16	part3_rom_gui9_addr;		// 000h
		u16	part4_rom_wifi7_addr;		// 002h
		u16	part34_gui_wifi_crc16;		// 004h
		u16	part12_boot_crc16;			// 006h
		u32	fw_identifier;				// 008h
		u16	part1_rom_boot9_addr;		// 00Ch
		u16	part1_ram_boot9_addr;		// 00Eh
		u16	part2_rom_boot7_addr;		// 010h
		u16	part2_ram_boot7_addr;		// 012h
		u16	shift_amounts;				// 014h
		u16	part5_data_gfx_addr;		// 016h

		u8	fw_timestamp[5];			// 018h
		u8	console_type;				// 01Dh
		u16	unused1;					// 01Eh
		u16	user_settings_offset;		// 020h
		u16	unknown1;					// 022h
		u16	unknown2;					// 024h
		u16	part5_crc16;				// 026h
		u16	unused2;					// 028h	- FFh filled 
	} header;

	u32		ARM9bootAddr;
	u32		ARM7bootAddr;
	bool	patched;
};

int copy_firmware_user_data( u8 *dest_buffer, const u8 *fw_data);
int NDS_CreateDummyFirmware(NDS_fw_config_data *user_settings);
void NDS_FillDefaultFirmwareConfigData(NDS_fw_config_data *fw_config);
void NDS_PatchFirmwareMAC();

struct fw_memory_chip
{
	u8 com;	//persistent command actually handled
	u32 addr;        //current address for reading/writing
	u8 addr_shift;   //shift for address (since addresses are transfered by 3 bytes units)
	u8 addr_size;    //size of addr when writing/reading

	BOOL write_enable;	//is write enabled ?

	u8 *data;       //memory data
	u32 size;       //memory size
	BOOL writeable_buffer;	//is "data" writeable ?
	int type; //type of Memory
	char *filename;
	FILE *fp;
	
	// needs only for firmware
	bool isFirmware;
	char userfile[MAX_PATH];
};


void fw_reset_com(fw_memory_chip *mc);
u8 fw_transfer(fw_memory_chip *mc, u8 data);
void mc_init(fw_memory_chip *mc, int type);    /* reset and init values for memory struct */
u8 *mc_alloc(fw_memory_chip *mc, u32 size);  /* alloc mc memory */
void mc_realloc(fw_memory_chip *mc, int type, u32 size);      /* realloc mc memory */
void mc_load_file(fw_memory_chip *mc, const char* filename); /* load save file and setup fp */
void mc_free(fw_memory_chip *mc);    /* delete mc memory */

#endif

