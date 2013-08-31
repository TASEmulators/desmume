/*
	Copyright (C) 2013 DeSmuME team

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

#ifndef _FS_NITRO_H_
#define _FS_NITRO_H_
#include <string>
#include "../types.h"

using namespace std;

enum FNT_TYPES
{
	FS_FILE_ENTRY = 0,
	FS_SUBDIR_ENTRY = 1,
	FS_END_SUBTABLE = 2,
	FS_RESERVED = 3,
};

#include "PACKED.h"
struct FAT_NITRO
{
	u32 start;
	u32 end;
	u32 size;
	u32	parentOffset;
	bool isOverlay;
	bool file;
	u32 sizeFile;
	u16 parentID;
	char filename[128];
};

struct FNT_MAIN
{
	u32 offset;
	u16 firstID;
	u16 parentID;
};

struct FNT_NITRO
{
	u32 offset;
	u16 firstID;
	u16 parentID;
	char filename[128];
};

struct OVR_NITRO
{
	u32 id;
	u32 RAMaddr;
	u32 RAMSize;
	u32 BSSSize;
	u32 start;
	u32 end;
	u32 fileID;
	u32 reserved;
};
#include "PACKED_END.h"

class FS_NITRO
{
private:
	bool inited;

	u32 FNameTblOff;
	u32 FNameTblSize;
	u32 FATOff;
	u32 FATSize;
	u32 FATEnd;
	u32 ARM9OverlayOff;
	u32 ARM9OverlaySize;
	u32 ARM7OverlayOff;
	u32 ARM7OverlaySize;

	u32 ARM9exeStart;
	u32 ARM9exeEnd;
	u32 ARM9exeSize;

	u32 ARM7exeStart;
	u32 ARM7exeEnd;
	u32 ARM7exeSize;

	u32 numFiles;
	u32 numDirs;
	u32 numOverlay7;
	u32 numOverlay9;

	u32 currentID;

	u8			*rom;
	FAT_NITRO	*fat;
	FNT_NITRO	*fnt;
	OVR_NITRO	*ovr9;
	OVR_NITRO	*ovr7;

	FNT_TYPES getFNTType(u8 type);
	bool loadFileTables();

public:
	FS_NITRO(u8 *cart_rom);
	~FS_NITRO();
	void destroy();

	bool getFileIdByAddr(u32 addr, u16 &id);
	bool getFileIdByAddr(u32 addr, u16 &id, u32 &offset);
	string getFileNameByID(u16 id);
	string getFullPathByFileID(u16 id);
	u32 getFileSizeById(u16 id);
	u32 getStartAddrById(u16 id);
	u32 getEndAddrById(u16 id);
	bool isARM9(u32 addr) { return ((addr >= ARM9exeStart) && (addr < ARM9exeEnd)); }
	bool isARM7(u32 addr) { return ((addr >= ARM7exeStart) && (addr < ARM7exeEnd)); }
	bool isFAT(u32 addr) { return ((addr >= FATOff) && (addr < FATEnd)); }
	bool rebuildFAT(u32 addr, u32 size, string pathData);
	bool rebuildFAT(string pathData);
	u32 getFATRecord(u32 addr);
	
	u32 getNumDirs() { return numDirs; }
	u32 getNumFiles() { return numFiles; }
};


#endif
