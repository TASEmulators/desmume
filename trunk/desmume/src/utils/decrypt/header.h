//taken from ndstool
//http://devkitpro.svn.sourceforge.net/viewvc/devkitpro/trunk/tools/nds/ndstool/include/header.h?revision=2447

/* header.h - this file is part of DeSmuME
 *
 * Copyright (C) 2005-2006 Rafael Vuijk
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _HEADER_H_
#define _HEADER_H_


#include "../../types.h"

#include "../../PACKED.h"
struct __PACKED Header
{
	char title[0xC];
	char gamecode[0x4];
	char makercode[2];
	unsigned char unitcode;							// product code. 0 = Nintendo DS
	unsigned char devicetype;						// device code. 0 = normal
	unsigned char devicecap;						// device size. (1<<n Mbit)
	unsigned char reserved1[0x9];					// 0x015..0x01D
	unsigned char romversion;
	unsigned char reserved2;						// 0x01F
	u32 arm9_rom_offset;					// points to libsyscall and rest of ARM9 binary
	u32 arm9_entry_address;
	u32 arm9_ram_address;
	u32 arm9_size;
	u32 arm7_rom_offset;
	u32 arm7_entry_address;
	u32 arm7_ram_address;
	u32 arm7_size;
	u32 fnt_offset;
	u32 fnt_size;
	u32 fat_offset;
	u32 fat_size;
	u32 arm9_overlay_offset;
	u32 arm9_overlay_size;
	u32 arm7_overlay_offset;
	u32 arm7_overlay_size;
	u32 rom_control_info1;					// 0x00416657 for OneTimePROM
	u32 rom_control_info2;					// 0x081808F8 for OneTimePROM
	u32 banner_offset;
	u16 secure_area_crc;
	u16 rom_control_info3;				// 0x0D7E for OneTimePROM
	u32 offset_0x70;						// magic1 (64 bit encrypted magic code to disable LFSR)
	u32 offset_0x74;						// magic2
	u32 offset_0x78;						// unique ID for homebrew
	u32 offset_0x7C;						// unique ID for homebrew
	u32 application_end_offset;			// rom size
	u32 rom_header_size;
	u32 offset_0x88;						// reserved... ?
	u32 offset_0x8C;

	// reserved
	u32 offset_0x90;
	u32 offset_0x94;
	u32 offset_0x98;
	u32 offset_0x9C;
	u32 offset_0xA0;
	u32 offset_0xA4;
	u32 offset_0xA8;
	u32 offset_0xAC;
	u32 offset_0xB0;
	u32 offset_0xB4;
	u32 offset_0xB8;
	u32 offset_0xBC;

	unsigned char logo[156];						// character data
	u16 logo_crc;
	u16 header_crc;

	// 0x160..0x17F reserved
	u32 offset_0x160;
	u32 offset_0x164;
	u32 offset_0x168;
	u32 offset_0x16C;
	unsigned char zero[0x90];
};
#include "../../PACKED_END.h"



struct Country
{
	const char countrycode;
	const char *name;
};

struct Maker
{
	const char *makercode;
	const char *name;
};

extern Country countries[];
extern int NumCountries;

extern Maker makers[];
extern int NumMakers;

unsigned short CalcHeaderCRC(Header &header);
unsigned short CalcLogoCRC(Header &header);
void FixHeaderCRC(char *ndsfilename);
void ShowInfo(char *ndsfilename);
int HashAndCompareWithList(char *filename, unsigned char sha1[]);
int DetectRomType(const Header& header, char* secure);
unsigned short CalcSecureAreaCRC(bool encrypt);

#define ROMTYPE_HOMEBREW	0
#define ROMTYPE_MULTIBOOT	1
#define ROMTYPE_NDSDUMPED	2	// decrypted secure area
#define ROMTYPE_ENCRSECURE	3
#define ROMTYPE_MASKROM		4	// unknown layout
#define ROMTYPE_INVALID		5	// rejected; can't be a supported rom

#endif
