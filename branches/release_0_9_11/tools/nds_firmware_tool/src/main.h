/* 	NDS Firmware tools

	Copyright 2009 DeSmuME team

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

#ifndef __MAIN_H_
#define __MAIN_H_
#include <windows.h>
#include <stdio.h>
#include "common.h"
#include "console.h"

#define _TITLE "NDS Firmware tool v0.0.3 by DeSmuME Team"
#define MAINCLASS "NDS_FIRMWARE_TOOLS"

typedef struct
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
} HEADER;
#endif