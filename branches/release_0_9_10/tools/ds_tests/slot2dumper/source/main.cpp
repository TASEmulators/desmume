/* 	Slot2 dumper

	Copyright (C) 2013 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <nds.h>
#include <fat.h>
#include <time.h>
#include <dirent.h>

#define HEADER_SIZE 0x100
#define SRAM_SIZE 0x00100000

PrintConsole topScreen;
PrintConsole bottomScreen;
bool fat = false;
u16 addr_1FFFC = 0xFFFF;
u16 addr_1FFFE = 0xFFFF;

void saveToDisk(u8 *data, u32 size, const char *prefix, bool specialROM = false)
{
	if (!fat) return;
	
	consoleSelect(&bottomScreen);
	char fname[1024] = {0};
	time_t unixTime = time(NULL);
	struct tm* timeStruct = gmtime((const time_t *)&unixTime);
	sprintf(fname, "slot2_%s_%i-%02i-%02i_%02i-%02i-%02i.bin", prefix, timeStruct->tm_year, timeStruct->tm_mon, timeStruct->tm_mday,
																timeStruct->tm_hour, timeStruct->tm_min, timeStruct->tm_sec);

	printf("Saving dump to %s...", fname);

	FILE *fp = fopen(fname, "wb");
	bool res = false;
	if (fp)
	{
		res = (fwrite(data, 1, HEADER_SIZE, fp) == HEADER_SIZE);
		if (res && specialROM)
		{
			res = (fwrite(&addr_1FFFC, 1, 2, fp) == 2);
			if (res)
				res = (fwrite(&addr_1FFFE, 1, 2, fp) == 2);
		}
		fclose(fp);
	}
	
	if (res)
		printf("Done\n");
	else
		printf("Failed\n");
}

void dumpSRAM()
{
	consoleSelect(&topScreen);
	consoleClear();
	sysSetCartOwner(BUS_OWNER_ARM9);
	u8 *buf = new u8[SRAM_SIZE];
	for (u32 i = 0; i < SRAM_SIZE; i++)
	{
		buf[i] = *(u8*)(0x0A000000 + i);
		iprintf("%02X", buf[i]);
	}
	saveToDisk(buf, SRAM_SIZE, "sram");

	delete [] buf;
}

void dumpROM(bool full)
{
	u32 size = full?0x00200000:HEADER_SIZE;
	consoleSelect(&topScreen);
	consoleClear();
	sysSetCartOwner(BUS_OWNER_ARM9);
	u8 *buf = new u8[size];
	for (u32 i = 0; i < size; i++)
	{
		buf[i] = *(u8*)(0x08000000 + i);
		iprintf("%02X", buf[i]);
	}
	
	if (!full)
	{
		addr_1FFFC = *(u16*)(0x0801FFFC);
		addr_1FFFE = *(u16*)(0x0801FFFE);
		iprintf("1FFFC:%04X, 1FFFE:%04X", addr_1FFFC, addr_1FFFE);
	}
	saveToDisk(buf, size, full?"full":"hdr", full?false:true);

	delete [] buf;
	
}

//---------------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------------
	int keys;
	
	
	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);

	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);

	consoleInit(&topScreen, 3,BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	consoleInit(&bottomScreen, 3,BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
	
	consoleSelect(&bottomScreen);
	iprintf("Slot2 dumper (GBA slot)\n");
	iprintf("(c) 2013 DeSmuME Team\n");
	iprintf("Press A to header dump\n");
	iprintf("Press B to full dump\n");
	iprintf("Press X to SRAM dump\n");
	
	consoleSetWindow(&bottomScreen, 0, 5, 32, 19);
	iprintf("\nInit FAT...");
	fat = fatInitDefault();
	if (fat) 
		iprintf("Done\n");
	else
		iprintf("Failed\n");
	
	while(1) {
		scanKeys();
		keys = keysDown();
		if(keys & KEY_A) dumpROM(false);
		else
			if(keys & KEY_B) dumpROM(true);
		else
			if(keys & KEY_X) dumpSRAM();

		swiWaitForVBlank();
	}

	return 0;
}
