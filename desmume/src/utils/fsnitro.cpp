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

#include <stdio.h>
#include <string>
#ifdef _MSC_VER
#include <direct.h>
#include <windows.h>
#define __mkdir(x) mkdir(x)
#else
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdint.h>
#define __mkdir(x) mkdir(x, 0777)
#endif
#include "fsnitro.h"

using namespace std;

#ifdef _WINDOWS
#define FS_DIRECTORY_DELIMITER_CHAR	"\\"
#else
#define FS_DIRECTORY_DELIMITER_CHAR	"/"
#endif

FS_NITRO::FS_NITRO(u8 *cart_rom)
{
	inited = false;
	numDirs = numFiles = numOverlay7 = numOverlay9 = currentID =0;
	fat = NULL;
	fnt = NULL;
	ovr9 = NULL;
	ovr7 = NULL;

	if (!cart_rom) return;
	
	rom = cart_rom;

	FNameTblOff		= *(u32*)(rom + 0x40);
	FNameTblSize	= *(u32*)(rom + 0x44);
	FATOff			= *(u32*)(rom + 0x48);
	FATSize			= *(u32*)(rom + 0x4C);
	
	ARM9OverlayOff	= *(u32*)(rom + 0x50);
	ARM9OverlaySize	= *(u32*)(rom + 0x54);
	ARM7OverlayOff	= *(u32*)(rom + 0x58);
	ARM7OverlaySize = *(u32*)(rom + 0x5C);

	ARM9exeSize		= *(u32*)(rom + 0x2C);
	ARM9exeStart	= *(u32*)(rom + 0x20);
	ARM9exeEnd		= (ARM9exeStart + ARM9exeSize);

	ARM7exeSize		= *(u32*)(rom + 0x3C);
	ARM7exeStart	= *(u32*)(rom + 0x30);
	ARM7exeEnd		= (ARM7exeStart + ARM7exeSize);

	//printf("ARM9exe %08Xh - %08Xh, size %08Xh\n", ARM9exeStart, ARM9exeEnd, ARM9exeSize);
	//printf("ARM7exe %08Xh - %08Xh, size %08Xh\n", ARM7exeStart, ARM7exeEnd, ARM7exeSize);

	if (FNameTblOff < 0x8000) return;
	if (FNameTblOff == 0) return;
	if (FATOff < 0x8000) return;
	if (FATSize == 0) return;

	numDirs = *(u16*)(rom + FNameTblOff + 6);
	numFiles = FATSize / 8;

	if (numFiles == 0 || numDirs == 0) 
	{
		numFiles = numDirs = 0;
		return;
	}

	FATEnd = (FATOff + FATSize);

	numOverlay9 = ARM9OverlaySize / sizeof(OVR_NITRO);
	numOverlay7 = ARM7OverlaySize / sizeof(OVR_NITRO);

	printf("Nitro File System:\n");
	printf("\t* FNT at 0x%08X, size 0x%08X\n", FNameTblOff, FNameTblSize);
	printf("\t* FAT at 0x%08X, size 0x%08X\n", FATOff, FATSize);
	printf("\t* ARM9 at Overlay 0x%08X, size 0x%08X\n", ARM9OverlayOff, ARM9OverlaySize);
	printf("\t* ARM7 at Overlay 0x%08X, size 0x%08X\n", ARM7OverlayOff, ARM7OverlaySize);
	printf("\t* ARM9 exe at %08X, size %08Xh\n", ARM9exeStart, ARM9exeSize);
	printf("\t* ARM7 exe at %08X, size %08Xh\n", ARM7exeStart, ARM7exeSize);
	printf("\t* Directories: %u\n", numDirs);
	printf("\t* Files %u\n", numFiles);
	printf("\t* ARM9 Overlays %u\n", numOverlay9);
	printf("\t* ARM7 Overlays %u\n", numOverlay7);

	fat = new FAT_NITRO[numFiles];
	fnt = new FNT_NITRO[numDirs];
	if (numOverlay7) ovr7 = new OVR_NITRO[numOverlay7];
	if (numOverlay9) ovr9 = new OVR_NITRO[numOverlay9];

	if (!loadFileTables())
	{
		destroy();
		printf("FSNITRO: Error loading file system tables\n");
		return;
	}

	inited = true;
}

FS_NITRO::~FS_NITRO()
{
	destroy();
}

void FS_NITRO::destroy()
{
	if (fat) { delete [] fat; fat = NULL; }
	if (fnt) { delete [] fnt; fnt = NULL; }
	if (ovr9) { delete [] ovr9; ovr9 = NULL; }
	if (ovr7) { delete [] ovr7; ovr7 = NULL; }
	numDirs = numFiles = numOverlay7 = numOverlay9 = currentID = 0;
	inited = false;
}

FNT_TYPES FS_NITRO::getFNTType(u8 type)
{
	if (type == 0x00) return FS_END_SUBTABLE;
	if (type == 0x80) return FS_RESERVED;
	if (type <  0x80) return FS_FILE_ENTRY;
	
	return FS_SUBDIR_ENTRY;
}

bool FS_NITRO::loadFileTables()
{
	if (!fnt) return false;
	if (!fat) return false;
	if (numOverlay7 && !ovr7) return false;
	if (numOverlay9 && !ovr9) return false;

	// ========= FAT (File Allocation Table)
	u32 *_FAT = (u32*)(rom + FATOff);
	for (u32 i = 0; i < numFiles; i++)
	{
		const u32 ofs = (i * 2);
		fat[i].start = *(_FAT + ofs);
		fat[i].end	 = *(_FAT + ofs + 1);
		fat[i].size	 = fat[i].end - fat[i].start;
		fat[i].sizeFile = fat[i].size;
		fat[i].isOverlay = false;
	}

	// ========= Overlays ARM9
	if (numOverlay9)
	{
		memcpy(ovr9, (rom + ARM9OverlayOff), ARM9OverlaySize);
		for (u32 i = 0 ; i < numOverlay9; i++)
		{
			char buf[129] = {0};
			memset(&buf[0], 0, sizeof(buf));
			fat[ovr9[i].fileID].isOverlay = true;
			sprintf(buf, "overlay_%04u.bin", ovr9[i].id);
			fat[ovr9[i].fileID].filename = buf;
		}
	}

	// ========= Overlays ARM7
	if (numOverlay7)
	{
		memcpy(ovr7, (rom + ARM7OverlayOff), ARM7OverlaySize);
		for (u32 i = 0 ; i < numOverlay7; i++)
		{
			char buf[129] = {0};
			memset(&buf[0], 0, sizeof(buf));
			fat[ovr7[i].fileID].isOverlay = true;
			sprintf(buf, "overlay_%04u.bin", ovr7[i].id);
			fat[ovr7[i].fileID].filename = buf;
		}
	}

	// ========= FNT (File Names Table)
	u8 *_FNT = (u8*)(rom + FNameTblOff);
	for (u32 i = 0; i < numDirs; i++)
	{
		memcpy(&fnt[i], _FNT, 8);
		//printf("FNT %04Xh: sub:%08Xh, 1st ID:%04xh, parentID:%04Xh\n", i, fnt[i].offset, fnt[i].firstID, fnt[i].parentID);
		_FNT += 8;
	}

	// ========= Read file structure
	u8			*sub = (u8*)(rom + FNameTblOff + fnt[0].offset);
	u8			*_end = (u8*)(rom + FNameTblOff + FNameTblSize - 1);
	u16			fileCount = fnt[0].firstID;
	u16			fntID = 0xF000;
	uintptr_t	*store = new uintptr_t[numDirs];
	
	if (!store) return false;
	memset(store, 0, sizeof(uintptr_t) * numDirs);

	fnt[0].filename = FS_DIRECTORY_DELIMITER_CHAR;
	fnt[0].parentID = 0xF000;

	//printf("FNT F000: Sub:%08Xh, 1st ID:%04xh, parentID:%04Xh <%s>\n", fnt[0].offset, fnt[0].firstID, fnt[0].parentID, fnt[0].filename);

	while (true)
	{
		u8 len = (sub[0] & 0x7F);
		FNT_TYPES type = getFNTType(sub[0]);

		if (type == FS_END_SUBTABLE)
		{
			//printf("********** End Subdir (%04Xh, parent %04X)\n", fntID, fnt[fntID & 0x0FFF].parentID);
			sub = (u8*)store[fntID & 0x0FFF];
			fntID = fnt[fntID & 0x0FFF].parentID;
			continue;
		}
		
		if (type == FS_SUBDIR_ENTRY)
		{
			//printf("********** Subdir Entry\n");
			char buf[129] = {0};
			memcpy(buf, (sub + 1), len); buf[len] = 0;
			sub += (len + 1);
			fntID = (*(u16*)sub);
			sub += 2;

			u32 id = (fntID & 0x0FFF);
			store[id] = (uintptr_t)sub;
			sub = (u8*)(rom + FNameTblOff + fnt[id].offset);
			fnt[id].filename = buf;

			//printf("FNT %04X: Sub:%08Xh, 1st ID:%04xh, parentID:%04Xh <%s>\n", fntID, fnt[id].offset, fnt[id].firstID, fnt[id].parentID, buf);
			continue;
		}

		if (type == FS_FILE_ENTRY)
		{
			//printf("********** File Entry\n");
			char buf[129] = {0};
			memcpy(buf, (sub + 1), len); buf[len] = 0;
			fat[fileCount].filename = buf;
			fat[fileCount].parentID = fntID;
			//printf("ID:%04Xh, len %03d, type %d, parentID %04X, filename: %s\n", fileCount, len, (u32)type, fntID, fat[fileCount].filename);
			sub += (len + 1);
			fileCount++;

			if (fileCount >= numFiles) break;
			continue;
		}

		if (type == FS_RESERVED)
		{
			printf("********** FS_RESERVED\n");
			break;
		}
	}
	delete [] store; store = NULL;

	return true;
}

// ======================= tools
bool FS_NITRO::rebuildFAT(u32 addr, u32 size, string pathData)
{
	if (!inited) return false;
	if (size == 0) return false;
	if (addr < FATOff) return false;
	if (addr > FATEnd) return false;

	const u32 startID = (addr - FATOff) / 8;
	const u32 endID = startID + (size / 8);
	//printf("Start rebuild FAT (start ID:%04Xh)\n", startID);

	u8 *romFAT = (u8*)(rom + addr);

	for (u32 i = startID; i < endID; i++)
	{
		if (i >= numFiles) break;
		string path = pathData + getFullPathByFileID(i);
		//printf("%04Xh - %s (%d)\n", i, path.c_str(), fat[i].size);
		fat[i].file = false;
		FILE *fp = fopen(path.c_str(), "rb");
		if (!fp) continue;
		fseek(fp, 0, SEEK_END);
		u32 size = ftell(fp);
		fclose(fp);

		fat[i].file = true;

		if (fat[i].size != size)
		{
			//printf("Different size: %s (ROM: %d, file %d)\n", path.c_str(), fat[i].size, size);
			fat[i].sizeFile = size;
		}
		else
			fat[i].sizeFile = fat[i].size;
	}

	return true;
}

bool FS_NITRO::rebuildFAT(string pathData)
{
	return rebuildFAT(FATOff, FATSize, pathData);
}

u32 FS_NITRO::getFATRecord(u32 addr)
{
	if (!inited) return 0xFFFFFFFF;
	if (addr < FATOff) return 0xFFFFFFFF;
	if (addr > FATEnd) return 0xFFFFFFFF;

	u32 id = (addr - FATOff) / 8;
	u32 offs = (addr - FATOff) % 8;

	if (offs == 0)
	{
		return fat[id].start;
	}
	else
	{
		if (fat[id].file)
			return (fat[id].start + fat[id].sizeFile);
		else
			return fat[id].end;
	}

	return 0xFFFFFFFF;
}

bool FS_NITRO::getFileIdByAddr(u32 addr, u16 &id)
{
	id = 0xFFFF;
	if (!inited) return false;

	u32 pos = currentID;
	while (true)
	{
		if ((addr >= fat[pos].start) && (addr < fat[pos].end))
		{
			id = pos;
			currentID = pos;
			return true;
		}
		pos++;
		if (pos >= numFiles) pos = 0;
		if (pos == currentID) break;
	}
	return false;
}

bool FS_NITRO::getFileIdByAddr(u32 addr, u16 &id, u32 &offset)
{
	id = 0xFFFF; offset = 0;
	if (!inited) return false;

	u32 pos = currentID;
	while (true)
	{
		if ((addr >= fat[pos].start) && (addr < fat[pos].end))
		{
			id = pos;
			offset = addr - fat[pos].start;
			currentID = pos;
			return true;
		}
		pos++;
		if (pos >= numFiles) pos = 0;
		if (pos == currentID) break;
	}

	return false;
}

string FS_NITRO::getDirNameByID(u16 id)
{
	if (!inited) return "";
	if ((id & 0xF000) != 0xF000) return "|file|";
	if ((id & 0x0FFF) > numDirs) return "<!ERROR invalid id>";

	return fnt[id & 0x0FFF].filename;
}

u16 FS_NITRO::getDirParrentByID(u16 id)
{
	if (!inited) return 0xFFFF;
	if ((id & 0xF000) != 0xF000) return 0xFFFF;
	if ((id & 0x0FFF) > numDirs) return 0xFFFF;

	return fnt[id & 0x0FFF].parentID;
}

string FS_NITRO::getFileNameByID(u16 id)
{
	if (!inited) return "";
	if ((id & 0xF000) == 0xF000) return "<directory>";
	if (id > numFiles) return "<!ERROR invalid id>";

	return fat[id].filename;
}

u16 FS_NITRO::getFileParentById(u16 id)
{
	if (!inited) return 0xFFFF;
	if ((id & 0xF000) == 0xF000) return 0xFFFF;
	if (id > numFiles) return 0xFFFF;

	return fat[id].parentID;
}

string FS_NITRO::getFullPathByFileID(u16 id, bool addRoot)
{
	if (!inited) return "";
	if (id > numFiles) return "<!ERROR invalid id>";
	string res = "";

	if (!fat[id].isOverlay)
	{
		u32 parentID = (fat[id].parentID & 0x0FFF);
		while (parentID)
		{
			res = fnt[parentID].filename + string(FS_DIRECTORY_DELIMITER_CHAR) + res;
			parentID = (fnt[parentID].parentID & 0x0FFF);
		}
		if (addRoot)
			res = string(FS_DIRECTORY_DELIMITER_CHAR) + string("data") + string(FS_DIRECTORY_DELIMITER_CHAR) + res;
	}
	else
	{
		if (addRoot)
			res = string(FS_DIRECTORY_DELIMITER_CHAR) + string("overlay") + string(FS_DIRECTORY_DELIMITER_CHAR);
	}

	res += fat[id].filename;
	return res;
}

u32 FS_NITRO::getFileSizeById(u16 id)
{
	if (!inited) return 0;
	if (id > numFiles) return 0;

	return (fat[id].size);
}

u32 FS_NITRO::getStartAddrById(u16 id)
{
	if (!inited) return 0;
	if (id > numFiles) return 0;

	return (fat[id].start);
}

u32 FS_NITRO::getEndAddrById(u16 id)
{
	if (!inited) return 0;
	if (id > numFiles) return 0;

	return (fat[id].end);
}

bool FS_NITRO::extract(u16 id, string to)
{
	printf("Extract to %s\n", to.c_str());

	FILE *fp = fopen(to.c_str(), "wb");
	if (fp)
	{
		fwrite((rom + fat[id].start), 1, fat[id].size, fp);
		fclose(fp);
		return true;
	}

	return false;
}

bool FS_NITRO::extractFile(u16 id, string to)
{
	if (!inited) return false;
	if (id > numFiles) return false;

	char curr_dir[MAX_PATH] = {0};
	getcwd(curr_dir, sizeof(curr_dir));
	chdir(to.c_str());
	extract(id, fat[id].filename);
	chdir(curr_dir);

	return true;
}

bool FS_NITRO::extractAll(string to, void (*callback)(u32 current, u32 num))
{
	if (!inited) return false;

	string dataDir = to + "data" + FS_DIRECTORY_DELIMITER_CHAR;
	string overlayDir = to + "overlay" + FS_DIRECTORY_DELIMITER_CHAR;
	__mkdir(dataDir.c_str());
	__mkdir(overlayDir.c_str());

	char curr_dir[MAX_PATH] = {0};
	getcwd(curr_dir, sizeof(curr_dir));
	chdir(dataDir.c_str());

	for (u32 i = 0; i < numDirs; i++)
	{
		string tmp = fnt[i].filename;
		u16 parent = (fnt[i].parentID) & 0x0FFF;

		while (parent)
		{
			tmp = fnt[parent].filename + string(FS_DIRECTORY_DELIMITER_CHAR) + tmp;
			parent = (fnt[parent].parentID) & 0x0FFF;
		}
		__mkdir(tmp.c_str());
	}

	chdir(dataDir.c_str());
	for (u32 i = 0; i < numFiles; i++)
	{
		if (fat[i].isOverlay) continue;
		string fname = getFullPathByFileID(i, false);
		extract(i, fname);
		if (callback)
			callback(i, numFiles);
	}

	chdir(overlayDir.c_str());
	for (u32 i = 0; i < numFiles; i++)
	{
		if (!fat[i].isOverlay) continue;
		extract(i, fat[i].filename);
	}

	chdir(curr_dir);

	return true;
}

