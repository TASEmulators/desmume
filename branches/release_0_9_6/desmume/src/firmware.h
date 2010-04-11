/*	Copyright (C) 2009 DeSmuME Team

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

#ifndef _FIRMWARE_H_
#define _FIRMWARE_H_
#include "common.h"

// the count of bytes copied from the firmware into memory 
#define NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT 0x70

class CFIRMWARE
{
private:
	u8		*tmp_data9;
	u8		*tmp_data7;
	u32		size9, size7;

	u32		keyBuf[0x412];
	u32		keyCode[3];

	bool	getKeyBuf();
	void	crypt64BitUp(u32 *ptr);
	void	crypt64BitDown(u32 *ptr);
	void	applyKeycode(u32 modulo);
	bool	initKeycode(u32 idCode, int level, u32 modulo);
	u16		getBootCodeCRC16();
	u32		decrypt(const u8 *in, u8* &out);
	u32		decompress(const u8 *in, u8* &out);

public:
	CFIRMWARE(): size9(0), size7(0), ARM9bootAddr(0), ARM7bootAddr(0), patched(0) {};
	
	bool load();

	struct HEADER
	{
		u16	part3_rom_gui9_addr;		// 000h
		u16	part4_rom_wifi7_addr;		// 002h
		u16	part34_gui_wifi_crc16;		// 004h
		u16	part12_boot_crc16;			// 006h
		u8	fw_identifier[4];			// 008h
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
int NDS_CreateDummyFirmware( struct NDS_fw_config_data *user_settings);
void NDS_FillDefaultFirmwareConfigData( struct NDS_fw_config_data *fw_config);

#endif

